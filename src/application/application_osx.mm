#include <common/exception.h>
#include <common/log.h>

#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSFileManager.h>
#import <AppKit/NSApplication.h>

using namespace engine::common;

namespace engine {
namespace application {

namespace
{

struct Pool
{
  NSAutoreleasePool* pool;

  Pool()
    : pool([[NSAutoreleasePool alloc] init])
  {

  }

  ~Pool()
  {
    [pool release];
  }
};

}

void init_application_osx()
{
  Pool pool;

  if (!NSApplicationLoad())
    throw Exception::format("Can't load application"); 

  NSBundle* main_bundle = [NSBundle mainBundle];

  if (!main_bundle)
  {
    throw Exception::format("Can't get main application bundle");
  }

  NSString* resources_path = [main_bundle resourcePath];

  if (![[NSFileManager defaultManager] changeCurrentDirectoryPath:resources_path])
    throw Exception::format("Can't set current dir to '%s'", [resources_path UTF8String]);

  engine_log_info("...set application current dir to '%s'", [resources_path UTF8String]);
}

}}
