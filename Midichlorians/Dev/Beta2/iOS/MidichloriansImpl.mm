/*******************************************************************************
 The MIT License (MIT)
 
 Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
           (c) 2015 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
           (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 *******************************************************************************/

#import "MidichloriansImpl.h"

#include <string>
#include <map>
#include <sstream>
#include <iomanip>
#include <utility>
#include <thread>
#include <mutex>
#include <queue>

#include <sys/xattr.h>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFURL.h>

#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSURLRequest.h>

#include <TargetConditionals.h>  // For `TARGET_OS_IPHONE`.
#if (TARGET_OS_IPHONE > 0)
// Works for all iOS devices, including iPad.
#import <UIKit/UIDevice.h>
#import <UIKit/UIScreen.h>
#import <UIKit/UIApplication.h>
#import <AdSupport/ASIdentifierManager.h>  // Need to add AdSupport.Framework to the project.
#endif  // (TARGET_OS_IPHONE > 0)

namespace current {
namespace midichlorians {

    // Conversion from [possible nullptr] `const char*` to `std::string`.
    static std::string ToStdString(const char* s) {
        return s ? s : "";
    }

#if (TARGET_OS_IPHONE > 0)
    // Conversion from [possible nil] `NSString` to `std::string`.
    static std::string ToStdString(NSString *nsString) {
        return nsString ? std::string([nsString UTF8String]) : "";
    }

    static std::string RectToString(CGRect const &rect) {
        return std::to_string(static_cast<int>(rect.origin.x)) + " " +
        std::to_string(static_cast<int>(rect.origin.y)) + " " +
        std::to_string(static_cast<int>(rect.size.width)) + " " +
        std::to_string(static_cast<int>(rect.size.height));
    }
#endif  // (TARGET_OS_IPHONE > 0)
    
    // Returns the timestamp of a file or directory.
    static uint64_t PathTimestampMillis(NSString *path) {
        NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:nil];
        if (attributes) {
            NSDate *date = [attributes objectForKey:NSFileModificationDate];
            return static_cast<uint64_t>([date timeIntervalSince1970] * 1000);
        } else {
            return 0ull;
        }
    }
    
    // Returns the <unique device id, true if it's the very-first app launch> tuple.
    static std::pair<std::string, bool> InstallationId() {
        const auto key = @"MidichloriansInstallationId";
        bool firstLaunch = false;
        NSUserDefaults *userDataBase = [NSUserDefaults standardUserDefaults];
        NSString *installationId = [userDataBase objectForKey:key];
        if (installationId == nil) {
            CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
            installationId = [@"iOS:"
                              stringByAppendingString:(NSString *)CFBridgingRelease(CFUUIDCreateString(kCFAllocatorDefault, uuid))];
            CFRelease(uuid);
            [userDataBase setValue:installationId forKey:key];
            [userDataBase synchronize];
            firstLaunch = true;
        }
        return std::make_pair([installationId UTF8String], firstLaunch);
    }
    
    namespace consumer {
        namespace thread_unsafe {
            
            class POSTviaHTTP {
            public:
                void SetServerUrl(const std::string &server_url) { server_url_ = server_url; }
                
                void SetDeviceId(const std::string &device_id) { device_id_ = device_id; }
                const std::string &GetDeviceId() const { return device_id_; }
                
                void SetClientId(const std::string &client_id) { client_id_ = client_id; }
                const std::string &GetClientId() const { return client_id_; }
                
                void OnMessage(const std::string &message) {
                    @autoreleasepool {
                        if (!server_url_.empty()) {
                            CURRENT_NSLOG(@"LogEvent HTTP: `%s`", message.c_str());
                            
                            NSMutableURLRequest *req = [NSMutableURLRequest
                                                        requestWithURL:[NSURL URLWithString:[NSString stringWithUTF8String:server_url_.c_str()]]];
                            // TODO(dkorolev): Add `cachePolicy:NSURLRequestReloadIgnoringLocalCacheData`. I can't make it compile.
                            
                            req.HTTPMethod = @"POST";
                            req.HTTPBody = [NSData dataWithBytes:message.data() length:message.length()];
                            [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
                            
                            NSHTTPURLResponse *res = nil;
                            NSError *err = nil;
                            NSData *url_data = [NSURLConnection sendSynchronousRequest:req returningResponse:&res error:&err];
                            // TODO(mzhurovich): Switch to `NSURLSession`.
                            
                            // TODO(dkorolev): A bit more detailed error handling.
                            // TODO(dkorolev): If the message queue is persistent, consider keeping unsent entries within it.
                            static_cast<void>(url_data);
                            if (!res) {
                                CURRENT_NSLOG(@"LogEvent HTTP: Fail.");
                            } else {
                                CURRENT_NSLOG(@"LogEvent HTTP: OK.");
                            }
                        } else {
                            CURRENT_NSLOG(@"LogEvent HTTP: No `server_url_` set.");
                        }
                    }  // @autoreleasepool
                }
                
            private:
                std::string server_url_;
                std::string device_id_;
                std::string client_id_;
            };
        }
        
        // A straightforward implementation for wrapping calls from multiple sources into a single thread,
        // preserving the order.
        // NOTE: In production, a more advanced implementation should be used,
        // with more efficient in-memory message queue and with optionally persistent storage of events.
        template <class T_SINGLE_THREADED_IMPL>
        class SimplestThreadSafeWrapper : public T_SINGLE_THREADED_IMPL {
        public:
            SimplestThreadSafeWrapper() : up_(true), thread_(&SimplestThreadSafeWrapper::Thread, this) {}
            ~SimplestThreadSafeWrapper() {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    up_ = false;
                }
                cv_.notify_all();
                thread_.join();
            }
            void OnMessage(const std::string &message) {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    queue_.push_back(message);
                }
                cv_.notify_all();
            }
            
        private:
            void Thread() {
                std::string message;
                while (true) {
                    // Mutex-locked section: Retrieve the next message, or wait for one.
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        if (!up_) {
                            // Terminating.
                            return;
                        }
                        if (queue_.empty()) {
                            cv_.wait(lock);
                            continue;
                        } else {
                            message = queue_.front();
                            queue_.pop_front();
                        }
                    }
                    // Mutex-free section: Process this message.
                    T_SINGLE_THREADED_IMPL::OnMessage(message);
                }
            }
            
            std::atomic_bool up_;
            std::deque<std::string> queue_;
            std::condition_variable cv_;
            std::mutex mutex_;
            std::thread thread_;
        };
        
        template <typename T>
        using ThreadSafeWrapper = SimplestThreadSafeWrapper<T>;
        
        using POSTviaHTTP = ThreadSafeWrapper<thread_unsafe::POSTviaHTTP>;
        
    }  // namespace consumer
    
    template <typename T>
    struct Singleton {
        static T &Instance() {
            static T instance;
            return instance;
        }
    };
    
    using Stats = consumer::POSTviaHTTP;

    struct EventsVariantDispatchedAssigner {
        explicit EventsVariantDispatchedAssigner(Variant<T_IOS_EVENTS>& variant) : variant_(variant) {}

        template <typename T>
        void operator()(const T& event) { variant_ = event; }

        Variant<T_IOS_EVENTS>& variant_;
    };
}  // namespace midichlorians
}  // namespace current

@implementation MidichloriansImpl

using namespace current::midichlorians;

+ (void)setup:(NSString *)serverUrl withLaunchOptions:(NSDictionary *)options {
    const auto installationId = InstallationId();
    
    Stats &instance = Singleton<Stats>::Instance();
    
    instance.SetServerUrl([serverUrl UTF8String]);
    instance.SetDeviceId(installationId.first);
    
    // Documents folder modification time can be interpreted as "app install time".
    // App bundle modification time can be interpreted as "app update time".
    
    if (installationId.second) {
        [MidichloriansImpl emit:iOSFirstLaunchEvent()];
    }
    
    [MidichloriansImpl
     emit:iOSAppLaunchEvent(
                            ToStdString([[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"] UTF8String]),
                            PathTimestampMillis([NSSearchPathForDirectoriesInDomains(
                                                                                     NSDocumentDirectory, NSUserDomainMask, YES) firstObject]),
                            PathTimestampMillis([[NSBundle mainBundle] executablePath]))];
    
#if (TARGET_OS_IPHONE > 0)
    UIDevice *device = [UIDevice currentDevice];
    UIScreen *screen = [UIScreen mainScreen];
    std::string preferredLanguages;
    for (NSString *lang in [NSLocale preferredLanguages]) {
        preferredLanguages += [lang UTF8String] + std::string(" ");
    }
    std::string preferredLocalizations;
    for (NSString *loc in [[NSBundle mainBundle] preferredLocalizations]) {
        preferredLocalizations += [loc UTF8String] + std::string(" ");
    }
    
    NSLocale *locale = [NSLocale currentLocale];
    std::string userInterfaceIdiom = "phone";
    if (device.userInterfaceIdiom == UIUserInterfaceIdiomPad) {
        userInterfaceIdiom = "pad";
    } else if (device.userInterfaceIdiom == UIUserInterfaceIdiomUnspecified) {
        userInterfaceIdiom = "unspecified";
    }
    
    std::map<std::string, std::string> info = {
        {"deviceName", ToStdString(device.name)},
        {"deviceSystemName", ToStdString(device.systemName)},
        {"deviceSystemVersion", ToStdString(device.systemVersion)},
        {"deviceModel", ToStdString(device.model)},
        {"deviceUserInterfaceIdiom", userInterfaceIdiom},
        {"screens", std::to_string([UIScreen screens].count)},
        {"screenBounds", RectToString(screen.bounds)},
        {"screenScale", std::to_string(screen.scale)},
        {"preferredLanguages", preferredLanguages},
        {"preferredLocalizations", preferredLocalizations},
        {"localeIdentifier", ToStdString([locale objectForKey:NSLocaleIdentifier])},
        {"calendarIdentifier", ToStdString([[locale objectForKey:NSLocaleCalendar] calendarIdentifier])},
        {"localeMeasurementSystem", ToStdString([locale objectForKey:NSLocaleMeasurementSystem])},
        {"localeDecimalSeparator", ToStdString([locale objectForKey:NSLocaleDecimalSeparator])},
    };
    if (device.systemVersion.floatValue >= 8.0) {
        info.emplace("screenNativeBounds", RectToString(screen.nativeBounds));
        info.emplace("screenNativeScale", std::to_string(screen.nativeScale));
    }
    
    if (NSClassFromString(@"ASIdentifierManager")) {
        ASIdentifierManager *manager = [ASIdentifierManager sharedManager];
        info.emplace("isAdvertisingTrackingEnabled", manager.isAdvertisingTrackingEnabled ? "YES" : "NO");
        if (manager.advertisingIdentifier) {
            info.emplace("advertisingIdentifier", ToStdString(manager.advertisingIdentifier.UUIDString));
        }
    }
    
    NSURL *url = [options objectForKey:UIApplicationLaunchOptionsURLKey];
    if (url) {
        info.emplace("UIApplicationLaunchOptionsURLKey", ToStdString([url absoluteString]));
    }
    NSString *source = [options objectForKey:UIApplicationLaunchOptionsSourceApplicationKey];
    if (source) {
        info.emplace("UIApplicationLaunchOptionsSourceApplicationKey", ToStdString(source));
    }
    
    if (!info.empty()) {
        [MidichloriansImpl emit:iOSDeviceInfo(info)];
    }
#else
    static_cast<void>(options);
#endif  // (TARGET_OS_IPHONE > 0)
}

+ (void)emit:(const iOSBaseEvent &)event {
    Stats &instance = Singleton<Stats>::Instance();
    Variant<T_IOS_EVENTS> v;
    EventsVariantDispatchedAssigner assigner(v);
    current::metaprogramming::RTTIDynamicCall<T_IOS_EVENTS>(event, assigner);
    Value<iOSBaseEvent>(v).device_id = instance.GetDeviceId();
    Value<iOSBaseEvent>(v).client_id = instance.GetClientId();
    Value<iOSBaseEvent>(v).user_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current::time::Now());
    instance.OnMessage(JSON(v));
}

// Identifies the user.
+ (void)identify:(NSString *)identifier {
    Stats &instance = Singleton<Stats>::Instance();
    instance.SetClientId(ToStdString([identifier UTF8String]));
}

@end
