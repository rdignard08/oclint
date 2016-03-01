#include "TestRuleOnCode.h"

#include "rules/cocoa/ObjCVerifyMethodIsUsedRule.cpp"

static string testMethodFromSuperThroughProperty = "\
@class NSString;                                    \n\
@interface NSObject                                 \n\
@property (readonly) NSString* description;         \n\
                                                    \n\
@end                                                \n\
                                                    \n\
@interface BaseObject : NSObject                    \n\
                                                    \n\
@end                                                \n\
                                                    \n\
@implementation BaseObject                          \n\
                                                    \n\
- (NSString*) description { return (void*)0; }      \n\
                                                    \n\
@end                                                \n\
                                                    \n\
";

static string testMethodFromSuper = "\
@class NSString;                                    \n\
@interface NSObject                                 \n\
- (NSString*) description;                          \n\
@end                                                \n\
                                                    \n\
@interface BaseObject : NSObject                    \n\
                                                    \n\
@end                                                \n\
                                                    \n\
@implementation BaseObject                          \n\
                                                    \n\
- (NSString*) description { return (void*)0; }      \n\
                                                    \n\
@end                                                \n\
                                                    \n\
";

static string testMethodFromProtocol = "\
@class NSString;                                    \n\
@interface NSObject                                 \n\
@end                                                \n\
                                                    \n\
@protocol NSObject                                  \n\
- (NSString*) description;                          \n\
@end                                                \n\
@interface BaseObject : NSObject <NSObject>         \n\
                                                    \n\
@end                                                \n\
                                                    \n\
@implementation BaseObject                          \n\
                                                    \n\
- (NSString*) description { return (void*)0; }      \n\
                                                    \n\
@end                                                \n\
                                                    \n\
";

static string testMethodUnused = "\
@interface NSObject                                 \n\
@end                                                \n\
typedef signed char BOOL;                           \n\
BOOL YES = 1;                                       \n\
                                                    \n\
@interface BaseObject : NSObject                    \n\
                                                    \n\
                                                    \n\
@end                                                \n\
                                                    \n\
@implementation BaseObject                          \n\
                                                    \n\
- (BOOL)isEqual:(id)obj {                           \n\
    return YES;                                     \n\
}                                                   \n\
                                                    \n\
@end                                                \n\
                                                    \n\
";

static string testMethodUsedInternally = "\
@interface NSObject                                 \n\
@end                                                \n\
typedef signed char BOOL;                           \n\
BOOL YES = 1;                                       \n\
                                                    \n\
@interface BaseObject : NSObject                    \n\
                                                    \n\
                                                    \n\
@end                                                \n\
                                                    \n\
@implementation BaseObject                          \n\
                                                    \n\
- (BOOL)isEqual:(id)obj {                           \n\
    return [self hash] == [obj hash];               \n\
}                                                   \n\
                                                    \n\
- (int) hash {                                      \n\
    return [self isEqual:(void*)0];                 \n\
}                                                   \n\
                                                    \n\
@end                                                \n\
                                                    \n\
";

static string testOneMethodUnused = "\
@interface NSObject                                 \n\
@end                                                \n\
typedef signed char BOOL;                           \n\
BOOL YES = 1;                                       \n\
                                                    \n\
@interface BaseObject : NSObject                    \n\
                                                    \n\
                                                    \n\
@end                                                \n\
                                                    \n\
@implementation BaseObject                          \n\
                                                    \n\
- (BOOL)isEqual:(id)obj {                           \n\
    return [self hash];                             \n\
}                                                   \n\
- (int)hash { return 0; }                           \n\
                                                    \n\
@end                                                \n\
                                                    \n\
";

TEST(ObjCVerifyMethodIsUsedRuleTest, PropertyTest)
{
    ObjCVerifyMethodIsUsedRule rule;
    EXPECT_EQ(1, rule.priority());
    EXPECT_EQ("method is used", rule.name());
    EXPECT_EQ("cocoa", rule.category());
}

TEST(ObjCVerifyMethodIsUsedRuleTest, MethodUsedSuperProperty)
{
    testRuleOnObjCCode(new ObjCVerifyMethodIsUsedRule(), testMethodFromSuperThroughProperty);
}

TEST(ObjCVerifyMethodIsUsedRuleTest, MethodUsedSuperMethod)
{
    testRuleOnObjCCode(new ObjCVerifyMethodIsUsedRule(), testMethodFromSuper);
}

TEST(ObjCVerifyMethodIsUsedRuleTest, MethodUsedProtocolMethod)
{
    testRuleOnObjCCode(new ObjCVerifyMethodIsUsedRule(), testMethodFromProtocol);
}

TEST(ObjCVerifyMethodIsUsedRuleTest, MethodNotUsed)
{
    testRuleOnObjCCode(new ObjCVerifyMethodIsUsedRule(),
                       testMethodUnused,
                       0, 13, 1, 15, 1,
                       "The method isEqual: was defined but not exported or referenced here");
}

TEST(ObjCVerifyMethodIsUsedRuleTest, MethodUsedInternally)
{
    testRuleOnObjCCode(new ObjCVerifyMethodIsUsedRule(), testMethodUsedInternally);
}

TEST(ObjCVerifyMethodIsUsedRuleTest, OneMethodNotUsed)
{
    testRuleOnObjCCode(new ObjCVerifyMethodIsUsedRule(),
                       testOneMethodUnused,
                       0, 13, 1, 15, 1,
                       "The method isEqual: was defined but not exported or referenced here");
}
