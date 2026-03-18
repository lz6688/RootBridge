#import <RootBridge.h>
#import "vendor/apple/dyld_priv.h"

@implementation RootBridge
+ (NSString *)getCallerPath {
    
    // 获取当前函数的返回地址(即调用者的下一条指令地址)
    // 编译器内置函数
    const void* ret_addr = __builtin_extract_return_addr(__builtin_return_address(0));

    if(ret_addr) {
        // 查找这个地址属于哪个已加载的二进制镜像(Mach-O文件)
        const char* ret_image_name = dyld_image_path_containing_address(ret_addr);

        if(ret_image_name) {
            // 将C字符串转换为Objective-C的NSString对象并返回
            return @(ret_image_name);
        }
    }

    return nil;
}


// 是否是无根模式
+ (BOOL)isJBRootless {
    static BOOL rootless = NO;
    static dispatch_once_t onceToken = 0;

    // 保证检测逻辑只执行一次
    dispatch_once(&onceToken, ^{
        // 获取当前执行文件的路径
        NSString* caller_path = [self getCallerPath];
        // 如果路径不是以"/Library"或"/usr"开头,则判定为“无根模式”
        rootless = !([caller_path hasPrefix:@"/Library"] || [caller_path hasPrefix:@"/usr"]);
    });

    return rootless;
}

// 获取越狱路径
+ (NSString *)getJBPath:(NSString *)path {
    // 条件1: 如果当前不是“无根模式” (isJBRootless 返回 NO)
    // 条件2: 如果输入的 path 为 nil
    // 条件3: 如果输入的 path 不是绝对路径 (不是以 '/' 开头)
    // 条件4: 如果输入的 path 已经以 "/var/jb" 开头
    // 满足以上任意一条，都直接返回原路径 path，不做任何处理。
    if(![self isJBRootless] || !path || ![path isAbsolutePath] || [path hasPrefix:@"/var/jb"]) {
        return path;
    }

    // 其他情况: 路径不是以上任何一种,则在路径前拼接上"/var/jb"
    return [@"/var/jb" stringByAppendingPathComponent:path];
}
@end
