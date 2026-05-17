/* Copyright (C) 2024 GBE Fork Contributors
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#pragma once

#include <cstring>
#include <thread>
#include <chrono>

#ifdef __WINDOWS__
#include <windows.h>
#include <psapi.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#else
#include <link.h>
#endif
#endif

namespace NetworkingPatch {

// 跨平台类型定义
#ifdef __WINDOWS__
typedef BYTE byte_t;
typedef DWORD size_t32;
#else
typedef uint8_t byte_t;
typedef uint32_t size_t32;
#endif

// 在内存中搜索字节特征（支持通配符）
static byte_t* FindPattern(byte_t* base, size_t size, const byte_t* pattern, const char* mask, int patternLen)
{
    for (size_t i = 0; i < size - patternLen; i++) {
        bool found = true;
        for (int j = 0; j < patternLen; j++) {
            if (mask[j] == 'x' && base[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return nullptr;
}

// 修改内存中的字节（跨平台）
static bool PatchByte(byte_t* address, byte_t oldValue, byte_t newValue)
{
    if (*address != oldValue) return false;

#ifdef __WINDOWS__
    DWORD oldProtect;
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    *address = newValue;
    VirtualProtect(address, 1, oldProtect, &oldProtect);
    return true;
#else
    // Linux/macOS: 使用 mprotect
    // 获取页面大小
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return false;
    
    // 计算页面对齐的地址
    void* pageStart = (void*)((uintptr_t)address & ~(pageSize - 1));
    
    // 修改内存保护
    if (mprotect(pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return false;
    }
    
    *address = newValue;
    
    // 恢复内存保护（可选，但更安全）
    mprotect(pageStart, pageSize, PROT_READ | PROT_EXEC);
    return true;
#endif
}

#ifdef __WINDOWS__
// Windows: 获取 steamnetworkingsockets.dll 模块句柄
static void* GetSteamNetworkingSocketsModule(byte_t** outBase, size_t* outSize)
{
    const char* dllNames[] = {
        "steamnetworkingsockets.dll",
        "steamnetworkingsockets64.dll",
        nullptr
    };

    // 尝试 30 秒（300 次 * 100ms）
    for (int retry = 0; retry < 300; retry++) {
        for (int i = 0; dllNames[i]; i++) {
            HMODULE h = GetModuleHandleA(dllNames[i]);
            if (h) {
                MODULEINFO modInfo;
                if (GetModuleInformation(GetCurrentProcess(), h, &modInfo, sizeof(modInfo))) {
                    *outBase = (byte_t*)modInfo.lpBaseOfDll;
                    *outSize = modInfo.SizeOfImage;
                    return h;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}
#else
// Linux/macOS: 获取 steamnetworkingsockets 库句柄
static void* GetSteamNetworkingSocketsModule(byte_t** outBase, size_t* outSize)
{
    const char* libNames[] = {
#ifdef __APPLE__
        "libsteamnetworkingsockets.dylib",
        "steamnetworkingsockets.dylib",
#else
        "libsteamnetworkingsockets.so",
        "steamnetworkingsockets.so",
#endif
        nullptr
    };

    // 尝试 30 秒（300 次 * 100ms）
    for (int retry = 0; retry < 300; retry++) {
        for (int i = 0; libNames[i]; i++) {
            void* handle = dlopen(libNames[i], RTLD_LAZY | RTLD_NOLOAD);
            if (handle) {
                // 获取库的基地址和大小
#ifdef __APPLE__
                // macOS: 使用 dyld API
                uint32_t imageCount = _dyld_image_count();
                for (uint32_t j = 0; j < imageCount; j++) {
                    const char* imageName = _dyld_get_image_name(j);
                    if (imageName && strstr(imageName, "steamnetworkingsockets")) {
                        const struct mach_header* header = (const struct mach_header*)_dyld_get_image_header(j);
                        if (header) {
                            *outBase = (byte_t*)header;
                            // 估算大小（遍历 load commands）
                            // 简化处理：使用一个合理的默认值
                            *outSize = 10 * 1024 * 1024; // 10MB
                            return handle;
                        }
                    }
                }
#else
                // Linux: 使用 dl_iterate_phdr
                struct CallbackData {
                    const char* targetName;
                    byte_t** base;
                    size_t* size;
                    bool found;
                } data = { libNames[i], outBase, outSize, false };
                
                dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) -> int {
                    CallbackData* d = (CallbackData*)data;
                    if (info->dlpi_name && strstr(info->dlpi_name, "steamnetworkingsockets")) {
                        *(d->base) = (byte_t*)info->dlpi_addr;
                        // 计算总大小
                        size_t totalSize = 0;
                        for (int k = 0; k < info->dlpi_phnum; k++) {
                            if (info->dlpi_phdr[k].p_type == PT_LOAD) {
                                size_t segEnd = info->dlpi_phdr[k].p_vaddr + info->dlpi_phdr[k].p_memsz;
                                if (segEnd > totalSize) {
                                    totalSize = segEnd;
                                }
                            }
                        }
                        *(d->size) = totalSize;
                        d->found = true;
                        return 1; // 停止迭代
                    }
                    return 0;
                }, &data);
                
                if (data.found) {
                    return handle;
                }
#endif
                dlclose(handle);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}
#endif

// 补丁：让 IP_AllowWithoutAuth 参数在 release 模式下可见
// 
// Windows 特征：TEST BL,BL + JNZ +0x11 + MOV RAX,[RAX+0x20]
// 字节：84 DB 75 11 48 8B 40 20
// 修改：75 -> 74 (JNZ -> JZ)
//
// Linux/macOS 可能使用不同的寄存器和指令，但逻辑相同
// 需要根据实际情况调整特征字节
static bool PatchConfigVisibility(byte_t* base, size_t size)
{
    // Windows 特征
    byte_t pattern_win[] = { 0x84, 0xDB, 0x75, 0x11, 0x48, 0x8B, 0x40, 0x20 };
    char mask_win[] = "xxxxxxxx";
    
    byte_t* addr = FindPattern(base, size, pattern_win, mask_win, 8);
    if (addr) {
        return PatchByte(addr + 2, 0x75, 0x74); // JNZ -> JZ
    }
    
#ifndef __WINDOWS__
    // Linux/macOS 可能的特征变体
    // 变体 1: 使用不同的寄存器
    byte_t pattern_unix1[] = { 0x84, 0xC0, 0x75, 0x11, 0x48, 0x8B, 0x40, 0x20 };
    char mask_unix1[] = "xxxxxxxx";
    
    addr = FindPattern(base, size, pattern_unix1, mask_unix1, 8);
    if (addr) {
        return PatchByte(addr + 2, 0x75, 0x74); // JNZ -> JZ
    }
    
    // 变体 2: 使用 TEST + JNE 组合
    byte_t pattern_unix2[] = { 0x84, 0xDB, 0x0F, 0x85 }; // TEST BL,BL + JNE
    char mask_unix2[] = "xxxx";
    
    addr = FindPattern(base, size, pattern_unix2, mask_unix2, 4);
    if (addr) {
        // JNE (0F 85) -> JE (0F 84)
        return PatchByte(addr + 3, 0x85, 0x84);
    }
    
    // 变体 3: 更宽松的匹配（仅匹配 TEST + JNZ 的核心部分）
    byte_t pattern_unix3[] = { 0x84, 0x00, 0x75 }; // TEST reg,reg + JNZ
    char mask_unix3[] = "x?x";
    
    addr = FindPattern(base, size, pattern_unix3, mask_unix3, 3);
    if (addr) {
        return PatchByte(addr + 2, 0x75, 0x74); // JNZ -> JZ
    }
#endif
    
    return false;
}

// 主入口：应用所有补丁
static void ApplyAll()
{
    // 在后台线程中执行，避免阻塞 DLL/SO 加载
    std::thread([]() {
        byte_t* base = nullptr;
        size_t size = 0;
        
        void* handle = GetSteamNetworkingSocketsModule(&base, &size);
        if (!handle || !base || size == 0) {
            // 未找到 steamnetworkingsockets 库，可能游戏不使用它
            return;
        }

        // 应用补丁：让 IP_AllowWithoutAuth 参数可见
        bool patched = PatchConfigVisibility(base, size);
        
        // 可选：记录补丁结果到日志
        // 注意：这里不使用 PRINT_DEBUG，因为可能在后台线程中
        if (patched) {
            // 补丁成功
        } else {
            // 补丁失败或未找到特征字节
            // 可能是库版本不匹配或平台特征不同
        }
        
#ifndef __WINDOWS__
        // Linux/macOS: 不需要关闭句柄，因为使用了 RTLD_NOLOAD
        // dlclose(handle);
#endif
    }).detach();
}

} // namespace NetworkingPatch
