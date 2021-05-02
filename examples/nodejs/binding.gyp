{
  'targets': [
    {
      'target_name': 'current-nodejs-integration-example',
      'sources': [ 'code.cpp' ],
      'include_dirs': ["<!@(node -p \"require('node-addon-api').include\")", "../../integrations/nodejs"],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
    }
  ]
}
