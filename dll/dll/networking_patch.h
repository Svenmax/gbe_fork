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
#include <cstdio>

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

// Debug logging helper
static void DebugLog(const char* format, ...)
{
    // Try multiple possible log file paths
    const char* logPaths[] = {
        "C:\\Users\\Public\\gbe_networking_patch.log",
        "gbe_networking_patch.log",  // Current directory
        nullptr
    };
    
    FILE* file = nullptr;
    for (int i = 0; logPaths[i] && !file; i++) {
        file = std::fopen(logPaths[i], "a");
    }
    
    if (!file) return;
    
    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::fprintf(file, "[%s] ", std::ctime(&time));
    
    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
    std::fprintf(file, "\n");
    std::fflush(file);  // Flush to disk immediately
    std::fclose(file);
}

// Cross-platform type aliases
#ifdef __WINDOWS__
typedef BYTE byte_t;
typedef DWORD size_t32;
#else
typedef uint8_t byte_t;
typedef uint32_t size_t32;
#endif

// Search for a byte pattern in memory (supports wildcards)
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

// Patch a byte in memory (cross-platform)
static bool PatchByte(byte_t* address, byte_t oldValue, byte_t newValue)
{
    if (*address != oldValue) {
        DebugLog("[PATCH] Byte mismatch at %p: expected 0x%02X, found 0x%02X", address, oldValue, *address);
        return false;
    }

#ifdef __WINDOWS__
    DWORD oldProtect;
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        DebugLog("[PATCH] VirtualProtect failed: error %lu", GetLastError());
        return false;
    }
    *address = newValue;
    VirtualProtect(address, 1, oldProtect, &oldProtect);
    DebugLog("[PATCH] Successfully patched byte at %p: 0x%02X -> 0x%02X", address, oldValue, newValue);
    return true;
#else
    // Linux/macOS: use mprotect
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        DebugLog("[PATCH] Failed to get page size");
        return false;
    }
    
    void* pageStart = (void*)((uintptr_t)address & ~(pageSize - 1));
    
    if (mprotect(pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        DebugLog("[PATCH] mprotect failed: errno %d", errno);
        return false;
    }
    
    *address = newValue;
    mprotect(pageStart, pageSize, PROT_READ | PROT_EXEC);
    DebugLog("[PATCH] Successfully patched byte at %p: 0x%02X -> 0x%02X", address, oldValue, newValue);
    return true;
#endif
}

#ifdef __WINDOWS__
// Windows: get steamnetworkingsockets.dll module handle
static void* GetSteamNetworkingSocketsModule(byte_t** outBase, size_t* outSize)
{
    const char* dllNames[] = {
        "steamnetworkingsockets.dll",
        "steamnetworkingsockets64.dll",
        nullptr
    };

    DebugLog("[INIT] Searching for steamnetworkingsockets DLL...");
    
    // Retry for 30 seconds (300 attempts * 100ms)
    for (int retry = 0; retry < 300; retry++) {
        for (int i = 0; dllNames[i]; i++) {
            HMODULE h = GetModuleHandleA(dllNames[i]);
            if (h) {
                MODULEINFO modInfo;
                if (GetModuleInformation(GetCurrentProcess(), h, &modInfo, sizeof(modInfo))) {
                    *outBase = (byte_t*)modInfo.lpBaseOfDll;
                    *outSize = modInfo.SizeOfImage;
                    DebugLog("[INIT] Found %s at base %p, size %lu bytes", dllNames[i], *outBase, *outSize);
                    return h;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    DebugLog("[INIT] Failed to find steamnetworkingsockets DLL after 30 seconds");
    return nullptr;
}
#else
// Linux/macOS: get steamnetworkingsockets library handle
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

    DebugLog("[INIT] Searching for steamnetworkingsockets library...");
    
    // Retry for 30 seconds (300 attempts * 100ms)
    for (int retry = 0; retry < 300; retry++) {
        for (int i = 0; libNames[i]; i++) {
            void* handle = dlopen(libNames[i], RTLD_LAZY | RTLD_NOLOAD);
            if (handle) {
#ifdef __APPLE__
                uint32_t imageCount = _dyld_image_count();
                for (uint32_t j = 0; j < imageCount; j++) {
                    const char* imageName = _dyld_get_image_name(j);
                    if (imageName && strstr(imageName, "steamnetworkingsockets")) {
                        const struct mach_header* header = (const struct mach_header*)_dyld_get_image_header(j);
                        if (header) {
                            *outBase = (byte_t*)header;
                            *outSize = 10 * 1024 * 1024; // 10MB
                            DebugLog("[INIT] Found %s at base %p", imageName, *outBase);
                            return handle;
                        }
                    }
                }
#else
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
                        DebugLog("[INIT] Found %s at base %p, size %zu bytes", info->dlpi_name, *(d->base), *(d->size));
                        return 1;
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
    
    DebugLog("[INIT] Failed to find steamnetworkingsockets library after 30 seconds");
    return nullptr;
}
#endif

// Patch: make IP_AllowWithoutAuth visible in release mode
static bool PatchConfigVisibility(byte_t* base, size_t size)
{
    DebugLog("[PATCH] Starting pattern search in %zu bytes...", size);
    
    // Windows pattern
    byte_t pattern_win[] = { 0x84, 0xDB, 0x75, 0x11, 0x48, 0x8B, 0x40, 0x20 };
    char mask_win[] = "xxxxxxxx";
    
    byte_t* addr = FindPattern(base, size, pattern_win, mask_win, 8);
    if (addr) {
        DebugLog("[PATCH] Found Windows pattern at offset %p", (void*)(addr - base));
        return PatchByte(addr + 2, 0x75, 0x74);
    }
    
#ifndef __WINDOWS__
    // Possible Linux/macOS pattern variants
    byte_t pattern_unix1[] = { 0x84, 0xC0, 0x75, 0x11, 0x48, 0x8B, 0x40, 0x20 };
    char mask_unix1[] = "xxxxxxxx";
    
    addr = FindPattern(base, size, pattern_unix1, mask_unix1, 8);
    if (addr) {
        DebugLog("[PATCH] Found Unix pattern variant 1 at offset %p", (void*)(addr - base));
        return PatchByte(addr + 2, 0x75, 0x74);
    }
    
    byte_t pattern_unix2[] = { 0x84, 0xDB, 0x0F, 0x85 };
    char mask_unix2[] = "xxxx";
    
    addr = FindPattern(base, size, pattern_unix2, mask_unix2, 4);
    if (addr) {
        DebugLog("[PATCH] Found Unix pattern variant 2 at offset %p", (void*)(addr - base));
        return PatchByte(addr + 3, 0x85, 0x84);
    }
    
    byte_t pattern_unix3[] = { 0x84, 0x00, 0x75 };
    char mask_unix3[] = "x?x";
    
    addr = FindPattern(base, size, pattern_unix3, mask_unix3, 3);
    if (addr) {
        DebugLog("[PATCH] Found Unix pattern variant 3 at offset %p", (void*)(addr - base));
        return PatchByte(addr + 2, 0x75, 0x74);
    }
#endif
    
    DebugLog("[PATCH] No matching pattern found");
    return false;
}

// Main entry point: apply all patches
static void ApplyAll()
{
    DebugLog("[MAIN] NetworkingPatch::ApplyAll() called");
    
    // Run in a background thread to avoid blocking DLL/SO loading
    std::thread([]() {
        DebugLog("[THREAD] Patch thread started");
        
        byte_t* base = nullptr;
        size_t size = 0;
        
        void* handle = GetSteamNetworkingSocketsModule(&base, &size);
        if (!handle || !base || size == 0) {
            DebugLog("[THREAD] Failed to get module information");
            return;
        }

        // Apply patch: make IP_AllowWithoutAuth visible
        bool patched = PatchConfigVisibility(base, size);
        
        if (patched) {
            DebugLog("[THREAD] Patch applied successfully!");
        } else {
            DebugLog("[THREAD] Patch failed - pattern not found or byte mismatch");
        }
        
#ifndef __WINDOWS__
        // Linux/macOS: no need to close handle because RTLD_NOLOAD is used
#endif
    }).detach();
}

} // namespace NetworkingPatch
