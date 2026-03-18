/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _MACH_O_DYLD_PRIV_H_
#define _MACH_O_DYLD_PRIV_H_

#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <Availability.h>
#include <TargetConditionals.h>
#include <mach-o/dyld.h>
#include <uuid/uuid.h>

#if __cplusplus
extern "C" {
#endif /* __cplusplus */



//
// libSystem.dylib 与 dyld 之间的私有接口
//

// 在 fork 之前（父进程中）调用，用于锁定 dyld 内部数据结构，防止在 fork 瞬间因并发操作导致状态不一致。
extern void _dyld_atfork_prepare(void);
// 在 fork 之后（父进程中）调用，用于释放 _dyld_atfork_prepare() 中加的锁，恢复 dyld 的正常操作。
extern void _dyld_atfork_parent(void);
// 在 fork 之后（子进程中）调用，用于重置子进程中的 dyld 状态（例如清理父进程继承的锁、重置线程局部存储等），确保子进程能安全使用动态库。
extern void _dyld_fork_child(void);


// 定义函数指针
typedef void (*_dyld_objc_notify_mapped)(unsigned count, const char* const paths[], const struct mach_header* const mh[]);
typedef void (*_dyld_objc_notify_init)(const char* path, const struct mach_header* mh);
typedef void (*_dyld_objc_notify_unmapped)(const char* path, const struct mach_header* mh);


//
// Note: 仅用于 Objective-C 运行时使用
// 注册处理程序,以便在对象编码图像被映射,解映射以及初始化时调用这些处理程序.
// 那些为动态库（dylib）编译的图像会自动增加引用计数,
// 因此 Objective-C 不再需要通过调用 dlopen() 来防止这些图像被卸载。
// 在调用 _dyld_objc_notify_register() 时,
// dyld 会将已加载的 Objective-C 图像传递给"映射"函数.
// 在任何后续的 dlopen() 调用中，dyld 也会调用"映射"函数.
// 当 dyld 要调用初始化器(即该图像中的初始化方法)时,dyld会调用"初始化"函数.
// 这就是 Objective-C 在该图像中调用任何 +load 方法的时候的情况.
//
void _dyld_objc_notify_register(_dyld_objc_notify_mapped    mapped,
                                _dyld_objc_notify_init      init,
                                _dyld_objc_notify_unmapped  unmapped);


//
// 获取给定已加载的机器头信息的Slide 
// Mac OS X 10.6 及更高版本
//
extern intptr_t _dyld_get_image_slide(const struct mach_header* mh);



struct dyld_unwind_sections
{
	const struct mach_header*		mh;
	const void*						dwarf_section;
	uintptr_t						dwarf_section_length;
	const void*						compact_unwind_section;
	uintptr_t						compact_unwind_section_length;
};


//
// 当且仅当某个已加载的 Mach-O 图像中包含"addr"时，该函数返回真。
//	info->mh							包含地址的图像的机器头信息
//  info->dwarf_section					指向 __TEXT/__eh_frame 部分起始位置的指针
//  info->dwarf_section_length			__TEXT/__eh_frame 部分的长度
//  info->compact_unwind_section		指向 __TEXT/__unwind_info 部分起始位置的指针
//  info->compact_unwind_section_length	__TEXT/__unwind_info 部分的长度
//
// 在 Mac OS X 10.6 及更高版本中存在 
#if !__USING_SJLJ_EXCEPTIONS__
extern bool _dyld_find_unwind_sections(void* addr, struct dyld_unwind_sections* info);
#endif


//
// 这是一种经过优化的dladdr()函数形式,它仅返回 dli_fname 字段。
//
// 在 Mac OS X 10.6 及更高版本中存在 
extern const char* dyld_image_path_containing_address(const void* addr);


//
// 这是一种经过优化的dladdr()函数形式,它仅返回 dli_fbase 字段.
// 返回 NULL, 如果地址不在 dyld 跟踪的任何图像中。
//
// 在 Mac OS X 10.11 及更高版本中存在 
extern const struct mach_header* dyld_image_header_containing_address(const void* addr);

//
// 返回进程的机器头信息
//
// 在 Mac OS X 10.16 及更高版本中存在 
extern const struct mach_header* _dyld_get_prog_image_header(void);

typedef uint32_t dyld_platform_t;

typedef struct {
    dyld_platform_t platform;
    uint32_t        version;
} dyld_build_version_t;

// 返回该进程的当前运行平台
extern dyld_platform_t dyld_get_active_platform(void);

// 基础平台是指那些带有版本号的平台(如macOS,iOS,watchOS,tvOS,bridgeOS)
// 所有其他平台都与一个基础平台相连接,以便进行版本检查.

// 其目的是让操作系统中的大部分代码都使用已设定的版本常量,这样就能正确处理机密信息以及未来的相关情况.
// platforms. For example:

//  if (dyld_program_sdk_at_least(dyld_fall_2018_os_versions)) {
//      New behaviour for programs built against the iOS 12, tvOS 12, watchOS 5, macOS 10.14, or bridgeOS 3 (or newer) SDKs
//  } else {
//      Old behaviour
//  }

// 在需要更精确控制的情况下（例如在不同年份被添加到各种平台中的 API）
// 可以使用操作系统特定的值来替代.与版本设置的常量不同平台特定的值永远不会是这样
// 如果正在运行的二进制文件与测试所针对的平台一致,则返回"真",这样就可以为特定平台构建相应的条件了
// 以及在不同时间发布的那些版本. For example:

//  if (dyld_program_sdk_at_least(dyld_platform_version_iOS_12_0)
//      || dyld_program_sdk_at_least(dyld_platform_version_watchOS_6_0)) {
//      New behaviour for programs built against the iOS 12 (fall 2018), watchOS 6 (fall 2019) (or newer) SDKs
//  } else {
//      Old behaviour all other platforms, as well as older iOSes and watchOSes
//  }

extern dyld_platform_t dyld_get_base_platform(dyld_platform_t platform);

// SPI 会询问该平台是否为模拟平台
extern bool dyld_is_simulator_platform(dyld_platform_t platform);

// 获取一个版本,并返回该图像是否是基于该 SDK 或更高版本构建的.
// 对于多平台的 mach-o 文件,它会根据当前运行的平台进行测试.
extern bool dyld_sdk_at_least(const struct mach_header* mh, dyld_build_version_t version);

// 获取一个版本,并返回该图像是否是使用该minos版本或更高版本构建而成的.
// 对于多平台的 mach-o 文件,它会根据当前运行的平台进行测试.
extern bool dyld_minos_at_least(const struct mach_header* mh, dyld_build_version_t version);

// 与主可执行文件一同运行的前两个功能的便捷版本
extern bool dyld_program_sdk_at_least(dyld_build_version_t version);
extern bool dyld_program_minos_at_least(dyld_build_version_t version);

// Function that walks through the load commands and calls the internal block for every version found
// Intended as a fallback for very complex (and rare) version checks, or for tools that need to
// print our everything for diagnostic reasons
extern void dyld_get_image_versions(const struct mach_header* mh, void (^callback)(dyld_platform_t platform, uint32_t sdk_version, uint32_t min_version));

// Convienence constants for dyld version SPIs.

// Because we now have so many different OSes with different versions these version set values are intended to
// to provide a more convenient way to version check. They may be used instead of platform specific version in
// dyld_sdk_at_least(), dyld_minos_at_least(), dyld_program_sdk_at_least(), and dyld_program_minos_at_least().
// Since they are references into a lookup table they MUST NOT be used by any code that does not ship as part of
// the OS, as the values may change and the tables in older OSes may not have the necessary values for back
// deployed binaries. These values are future proof against new platforms being added, and any checks against
// platforms that did not exist at the epoch of a version set will return true since all versions of that platform
// are inherently newer.

//@VERSION_DEFS@

//
// This finds the SDK version a binary was built against.
// Returns zero on error, or if SDK version could not be determined.
//
// Exists in Mac OS X 10.8 and later 
// Exists in iOS 6.0 and later
extern uint32_t dyld_get_sdk_version(const struct mach_header* mh);


//
// This finds the SDK version that the main executable was built against.
// Returns zero on error, or if SDK version could not be determined.
//
// Note on watchOS, this returns the equivalent iOS SDK version number
// (i.e an app built against watchOS 2.0 SDK returne 9.0).  To see the
// platform specific sdk version use dyld_get_program_sdk_watch_os_version().
//
// Exists in Mac OS X 10.8 and later 
// Exists in iOS 6.0 and later
extern uint32_t dyld_get_program_sdk_version(void);

// #if TARGET_OS_WATCH
// // watchOS only.
// // This finds the Watch OS SDK version that the main executable was built against.
// // Exists in Watch OS 2.0 and later
// extern uint32_t dyld_get_program_sdk_watch_os_version(void);


// // watchOS only.
// // This finds the Watch min OS version that the main executable was built to run on.
// // Note: dyld_get_program_min_os_version() returns the iOS equivalent (e.g. 9.0)
// //       whereas this returns the raw watchOS version (e.g. 2.0).
// // Exists in Watch OS 3.0 and later
// extern uint32_t dyld_get_program_min_watch_os_version(void);
// #endif

// #if TARGET_OS_BRIDGE
// // bridgeOS only.
// // This finds the bridgeOS SDK version that the main executable was built against.
// // Exists in bridgeOSOS 2.0 and later
// extern uint32_t dyld_get_program_sdk_bridge_os_version(void);

// // bridgeOS only.
// // This finds the Watch min OS version that the main executable was built to run on.
// // Note: dyld_get_program_min_os_version() returns the iOS equivalent (e.g. 9.0)
// //       whereas this returns the raw bridgeOS version (e.g. 2.0).
// // Exists in bridgeOS 2.0 and later
// extern uint32_t dyld_get_program_min_bridge_os_version(void);
// #endif

//
// This finds the min OS version a binary was built to run on.
// Returns zero on error, or if no min OS recorded in binary.
//
// Exists in Mac OS X 10.8 and later 
// Exists in iOS 6.0 and later
extern uint32_t dyld_get_min_os_version(const struct mach_header* mh);


//
// This finds the min OS version the main executable was built to run on.
// Returns zero on error, or if no min OS recorded in binary.
//
// Exists in Mac OS X 10.8 and later 
// Exists in iOS 6.0 and later
extern uint32_t dyld_get_program_min_os_version(void);




//
// Returns if any OS dylib has overridden its copy in the shared cache
//
// Exists in iPhoneOS 3.1 and later 
// Exists in Mac OS X 10.10 and later
extern bool dyld_shared_cache_some_image_overridden(void);


	
//
// Returns if the process is setuid or is code signed with entitlements.
// NOTE: It is safe to call this prior to malloc being initialized.  This function
// is guaranteed to not call malloc, or depend on its state.
//
// Exists in Mac OS X 10.9 and later
extern bool dyld_process_is_restricted(void);



//
// Returns path used by dyld for standard dyld shared cache file for the current arch.
//
// Exists in Mac OS X 10.11 and later
extern const char* dyld_shared_cache_file_path(void);



//
// Returns if there are any inserted (via DYLD_INSERT_LIBRARIES) or interposing libraries.
//
// Exists in Mac OS X 10.15 and later
extern bool dyld_has_inserted_or_interposing_libraries(void);

//
// 如果dyld中包含针对特定标识符的修复方案,则返回true.此功能旨在用于预设重大变更的接口规范(SPI)阶段
// changes
//
// 适用于 macOS 10.16、iOS 14、tvOS 14、watchOS 7 及更高版本系统。

extern bool _dyld_has_fix_for_radar(const char *rdar);


//
// <rdar://problem/13820686> 以便让OpenGL能告知dyld当图像处理完毕后可以释放其占用的内存.
//
// 在 Mac OS X 10.9 及更高版本中存在
#define NSLINKMODULE_OPTION_CAN_UNLOAD                  0x20


//
// 更新指定图像上的所有绑定项. 
// 查找"替代"一词的使用情况，并将其修改为"被替代者"。
// NOTE: 这比通过 DYLD_INSERT_LIBRARIES 使用静态插件的方式要不那么安全
// 因为正在运行的程序可能已经将指针的值复制到了其他地方
// dyld 未知的那些位置.
//
struct dyld_interpose_tuple {
	const void* replacement;
	const void* replacee;
};
extern void dyld_dynamic_interpose(const struct mach_header* mh, const struct dyld_interpose_tuple array[], size_t count);


struct dyld_shared_cache_dylib_text_info {
	uint64_t		version;		// current version 2
	// following fields all exist in version 1
	uint64_t		loadAddressUnslid;
	uint64_t		textSegmentSize; 
	uuid_t			dylibUuid;
	const char*		path;			// pointer invalid at end of iterations
	// following fields all exist in version 2
	uint64_t        textSegmentOffset;  // offset from start of cache
};
typedef struct dyld_shared_cache_dylib_text_info dyld_shared_cache_dylib_text_info;


#ifdef __BLOCKS__
//
// 给定一个 dyld 共享缓存文件的 UUID，此函数将尝试查找该缓存文件。
// 调用该函数并检查结果，然后遍历所有图像，返回每个图像的相关信息。成功时返回 0 。
//
// 适用于 Mac OS X 10.11 及更高版本
//            iOS 9.0 及更高版本
extern int dyld_shared_cache_iterate_text(const uuid_t cacheUuid, void (^callback)(const dyld_shared_cache_dylib_text_info* info));


//
// 给定一个dyld共享缓存文件的UUID,以及一个以空字符结尾的用于搜索的额外目录路径数组
// 此功能将扫描标准目录和附加目录,以查找与 UUID 相匹配的缓存文件.
// 如果找到匹配项,则会遍历所有图像,并返回每个图像的相关信息.成功时返回0 
//
// 在 Mac OS X 10.12 及更高版本中存在
//            iOS 10.0 及更高版本
extern int dyld_shared_cache_find_iterate_text(const uuid_t cacheUuid, const char* extraSearchDirs[], void (^callback)(const dyld_shared_cache_dylib_text_info* info));
#endif /* __BLOCKS */


//
// 如果指定的地址范围位于dyld所管理的内存中,则返回真
// 该内容被设置为只读模式,并且永远不会被卸载.
//
// 在 Mac OS X 10.12 及更高版本中存在
//            iOS 10.0 及更高版本
extern bool _dyld_is_memory_immutable(const void* addr, size_t length);


//
// 获取给定图像的 UUID（通过 LC_UUID 加载命令获取）。
// 如果 LC_UUID 不存在或者 Mach 头格式不正确，则返回 false 。
//
// 在 Mac OS X 10.12 及更高版本中存在
// 在 iOS 10.0 及更高版本中存在
extern bool _dyld_get_image_uuid(const struct mach_header* mh, uuid_t uuid);


//
// 获取当前进程中 dyld 共享缓存的 UUID。
// 如果当前进程中未使用 dyld 共享缓存，则返回 false 。
//
// 在 Mac OS X 10.12 及更高版本中存在
// 在 iOS 10.0 及更高版本中存在
extern bool _dyld_get_shared_cache_uuid(uuid_t uuid);


//
// 返回进程中的 dyld 缓存的起始地址,并将长度设置为缓存的大小.
// 如果该进程未使用 dyld 共享缓存,则返回 NULL .
//
// 在 Mac OS X 10.13 及更高版本中存在
// 在 iOS 11.0 及更高版本中存在
extern const void* _dyld_get_shared_cache_range(size_t* length);


//
// 返回值表示当前活动的 dyld 共享缓存是否已优化。
// Note: macOS 不使用优化过的缓存，因此总是会返回错误结果。
//
// 在 Mac OS X 10.15 及更高版本中存在
// 在 iOS 13.0 及更高版本中存在
extern bool _dyld_shared_cache_optimized(void);


//
// 如果当前活动的 dyld 共享缓存是本地构建的,则返回真.
//
// 在 Mac OS X 10.15 及更高版本中存在
// 在 iOS 13.0 及更高版本中存在
extern bool _dyld_shared_cache_is_locally_built(void);

//
// 如果给定的应用程序需要构建闭包，则返回真。
//
// 在 Mac OS X 10.15 及更高版本中存在
// 在 iOS 13.0 及更高版本中存在
extern bool dyld_need_closure(const char* execPath, const char* dataContainerRootDir);


struct dyld_image_uuid_offset {
    uuid_t                      uuid;
	uint64_t                    offsetInImage;
    const struct mach_header*   image;
};

//
// 给定一组地址，返回每个地址的相关信息。
// 常用用法是将地址数组作为参数传递，该数组是通过栈回溯生成的。
// 对于每个地址，返回该图像加载的位置、地址在图像中的偏移量以及图像的 UUID。
// 如果指定的地址对 dyld 未知，则所有字段都将返回零。
//
// 在 macOS 10.14 及更高版本中存在
// 在 iOS 12.0 及更高版本中存在
extern void _dyld_images_for_addresses(unsigned count, const void* addresses[], struct dyld_image_uuid_offset infos[]);


//
// 允许您注册一个回调函数，每当有图像加载时该函数就会被调用，并提供“mach_header*”、路径以及相关数据。
// 该图像是否可以在之后卸载。在调用 _dyld_register_for_image_loads() 函数时，会触发回调函数的执行。
// 对于当前加载的每一张图像，都执行一次此操作。
//
// 在 macOS 10.14 及更高版本中存在
// 在 iOS 12.0 及更高版本中存在
extern void _dyld_register_for_image_loads(void (*func)(const struct mach_header* mh, const char* path, bool unloadable));




//
// 允许您注册一个回调函数，该函数会在批量处理图像加载通知时被调用。在调用该函数期间
// "dyld_register_for_bulk_image_loads()" 函数中，回调函数会在一次性接收到所有已加载图像时被调用。
// 随后,在调用 dlopen() 函数时,回调函数会一次性被调用一次,传入所有新的图像数据
//
// 在 macOS 10.15 及更高版本中存在
// 在 iOS 13.0 及更高版本中存在
extern void _dyld_register_for_bulk_image_loads(void (*func)(unsigned imageCount, const struct mach_header* mhs[], const char* paths[]));


//
// DriverKit 主程序文件中不存在 LC_MAIN 标签.相反,DriverKit.framework文件夹中的初始化器会调用
// 将 _dyld_register_driverkit_main() 函数替换为一个函数指针,该指针由 dyld 调用,
// 以替代使用 LC_MAIN.
//
extern void _dyld_register_driverkit_main(void (*mainFunc)(void));


//
// 这与 _dyld_shared_cache_contains_path() 函数的功能类似，不同之处在于它会返回规范化的路径。
// 如果给定的路径存在于 dyld 共享缓存中，则返回该路径的规范化版本。
// 如果路径不存在于共享缓存中，则返回 NULL 。
//
// 在 macOS 10.16 及更高版本中存在
// 在 iOS 14.0 及更高版本中存在
extern const char* _dyld_shared_cache_real_path(const char* path);


//
// dyld 有多种模式。此函数返回当前进程的模式。
// dyld2 是经典的“解释器”方式运行。
// dyld3 通过将 dyld 需要执行的操作编译并缓存到“闭包”中来运行。
//
// 在 macOS 10.16 及更高版本中存在
// 在 iOS 14.0 及更高版本中存在
//
#define DYLD_LAUNCH_MODE_USING_CLOSURE               0x00000001     // 进程正在使用闭包
#define DYLD_LAUNCH_MODE_BUILT_CLOSURE_AT_LAUNCH     0x00000002     // 进程启动时构建闭包
#define DYLD_LAUNCH_MODE_CLOSURE_SAVED_TO_FILE       0x00000004     // 闭包已保存到文件
#define DYLD_LAUNCH_MODE_CLOSURE_FROM_OS             0x00000008     // 闭包已嵌入 dyld 缓存
#define DYLD_LAUNCH_MODE_MINIMAL_CLOSURE             0x00000010     // 闭包中没有修复内容
extern uint32_t _dyld_launch_mode(void);


//	
// 当 dyld 必须因为缺少必需的依赖 dylib 而终止进程时，
// 或者缺少符号时，dyld 会调用 abort_with_reason() 函数
// 使用以下错误代码之一。
//
#define DYLD_EXIT_REASON_DYLIB_MISSING          1
#define DYLD_EXIT_REASON_DYLIB_WRONG_ARCH       2
#define DYLD_EXIT_REASON_DYLIB_WRONG_VERSION    3
#define DYLD_EXIT_REASON_SYMBOL_MISSING         4
#define DYLD_EXIT_REASON_CODE_SIGNATURE         5
#define DYLD_EXIT_REASON_FILE_SYSTEM_SANDBOX    6
#define DYLD_EXIT_REASON_MALFORMED_MACHO        7
#define DYLD_EXIT_REASON_OTHER                  9

//
// 当 dyld 必须因为缺少必需的依赖 dylib 而终止进程时，
// 或者缺少符号时，dyld 会调用 abort_with_payload() 函数
// 使用以下错误代码之一。
//
struct dyld_abort_payload {
	uint32_t version;                   // first version is 1	
	uint32_t flags;                     // 0x00000001 means dyld terminated at launch, backtrace not useful
	uint32_t targetDylibPathOffset;     // offset in payload of path string to dylib that could not be loaded
	uint32_t clientPathOffset;          // offset in payload of path string to image requesting dylib
	uint32_t symbolOffset;              // offset in payload of symbol string that could not be found
	// string data
};
typedef struct dyld_abort_payload dyld_abort_payload;


// 这些全局变量是在 libdyld.dylib 中实现的
// 那些使用 crt1.o 的旧程序也会定义这些全局变量。
// 在运行旧程序时，dyld 中的那些内容是不会被使用的。
extern int          NXArgc;
extern const char** NXArgv;
extern       char** environ;       // POSIX says this not const, because it pre-dates const
extern const char*  __progname;


// 	仅由 libSystem_initializer 调用
extern void _dyld_initializer(void);

// 从未在源代码中出现过。由静态链接器使用，以实现延迟绑定功能。
extern void dyld_stub_binder(void) __asm__("dyld_stub_binder");

// 切勿在源代码中调用。此功能由闭包构建器使用，用于将缺失的延迟符号绑定到闭包中。
extern void _dyld_missing_symbol_abort(void);

// 	只有在被 objc 调用时才会检查 dyld 是否已为该选择器进行了唯一标识。
// 如果 dyld 已对某个值进行了唯一化处理，则返回该值；否则返回 nullptr 。
// Note, 此函数必须在 _dyld_objc_notify_register 之后被调用。
//
// 在 Mac OS X 10.15 及更高版本中存在
// 在 iOS 13.0 及更高版本中存在
extern const char* _dyld_get_objc_selector(const char* selName);


// Called only by objc to see if dyld has pre-optimized classes with this name.
// The callback will be called once for each class with the given name where
// isLoaded is true if that class is in a binary which has been previously passed
// to the objc load notifier.
// Note you can set stop to true to stop iterating.
// Also note, this function must be called after _dyld_objc_notify_register.
//
// 在 Mac OS X 10.15 及更高版本中存在
// 在 iOS 13.0 及更高版本中存在
extern void _dyld_for_each_objc_class(const char* className,
                                      void (^callback)(void* classPtr, bool isLoaded, bool* stop));


// 只有在被 objc 调用时才会检查 dyld 是否已对具有此名称的协议进行了预优化处理。
// 对于具有给定名称的每个协议，回调函数都会被调用一次。
// 如果该协议所对应的二进制文件之前已被传输过，则“isLoaded”为真。
// 回调函数会将该协议的指针作为参数传递。
// 如果该协议所对应的二进制文件之前未被传输过，则“isLoaded”为假。
// 	请注意，您可以将“stop”设置为“true”以停止迭代。
// 另外请注意，此功能必须在 _dyld_objc_notify_register 之后调用。
//
// 在 Mac OS X 10.15 及更高版本中存在
// 在 iOS 13.0 及更高版本中存在
extern void _dyld_for_each_objc_protocol(const char* protocolName,
                                         void (^callback)(void* protocolPtr, bool isLoaded, bool* stop));


// 在调用exit()之前,它会先调用cxa_finalize(),以便实现线程局部性。
// 对象的销毁会先于全局对象进行.
extern void _tlv_exit(void);

typedef enum {
    dyld_objc_string_kind
} DyldObjCConstantKind;

// CF constants such as CFString's can be moved in to a contiguous range of
// shared cache memory.  This returns true if the given pointer is to an object of
// the given kind.
//
// Exists in Mac OS X 10.16 and later
// Exists in iOS 14.0 and later
extern bool _dyld_is_objc_constant(DyldObjCConstantKind kind, const void* addr);


// 暂时将这些内容导出以让TAPI感到满意,直到ASan停止使用dyldVersionNumber为止.
extern double      dyldVersionNumber;
extern const char* dyldVersionString;

#if __cplusplus
}
#endif /* __cplusplus */

#endif /* _MACH_O_DYLD_PRIV_H_ */
