{
  'targets': [
    {
      'target_name': 'current-nodejs-external-server',
      'sources': [ 'js_native.cpp' ],
      "libraries": [ "../external/build/js_external.a" ],
      'include_dirs': ["<!@(node -p \"require('node-addon-api').include\")", "../../integrations/nodejs"],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
    }
  ]
}
