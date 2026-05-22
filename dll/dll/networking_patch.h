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

// Count pattern matches
static int CountPattern(byte_t* base, size_t size, const byte_t* pattern, const char* mask, int patternLen)
{
    int count = 0;
    for (size_t i = 0; i < size - patternLen; i++) {
        bool found = true;
        for (int j = 0; j < patternLen; j++) {
            if (mask[j] == 'x' && base[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) count++;
    }
    return count;
}

// Pattern definition structure
struct PatternDef {
    byte_t pattern[16];
    char mask[16];
    int len;
    int patchOffset;
    byte_t oldByte;
    byte_t newByte;
    const char* description;
};

// Fallback: search by string and context
static bool FallbackPatch(byte_t* base, size_t size)
{
    DebugLog("[FALLBACK] Attempting string-based search...");
    
    // Step 1: Find "IP_AllowWithoutAuth" string
    const char* targetStr = "IP_AllowWithoutAuth";
    int strLen = (int)strlen(targetStr);
    
    byte_t* strAddr = nullptr;
    for (size_t i = 0; i < size - strLen - 1; i++) {
        if (memcmp(base + i, targetStr, strLen) == 0 && base[i + strLen] == 0) {
            strAddr = base + i;
            break;
        }
    }
    
    if (!strAddr) {
        DebugLog("[FALLBACK] String 'IP_AllowWithoutAuth' not found");
        return false;
    }
    
    DebugLog("[FALLBACK] Found string at offset %p", (void*)(strAddr - base));
    
    // Step 2: Find pointer to this string (config struct at offset 0x08)
    uintptr_t strAbsAddr = (uintptr_t)strAddr;
    int configId = -1;
    
    for (size_t i = 0; i < size - sizeof(uintptr_t); i++) {
        if (*(uintptr_t*)(base + i) == strAbsAddr) {
            byte_t* structBase = base + i - 0x08;
            int id = *(int*)structBase;
            if (id > 0 && id < 256) {
                configId = id;
                DebugLog("[FALLBACK] Found config ID: 0x%02X", configId);
                break;
            }
        }
    }
    
    if (configId < 0) {
        DebugLog("[FALLBACK] Config ID not found");
        return false;
    }
    
    // Step 3: Search for TEST + conditional jump + MOV pattern
    for (size_t i = 0; i < size - 8; i++) {
        // Match TEST r8, r8 (84 xx)
        if (base[i] != 0x84) continue;
        
        // Match conditional jump JNZ(75) or JZ(74)
        byte_t jmpByte = base[i + 2];
        if (jmpByte != 0x75 && jmpByte != 0x74) continue;
        
        // Match MOV RAX,[RAX+0x20] or similar (48 8B 40 20)
        if (base[i + 4] != 0x48 || base[i + 5] != 0x8B ||
            base[i + 6] != 0x40 || base[i + 7] != 0x20) continue;
        
        // Verify: search backwards for configId reference
        bool hasIdRef = false;
        int searchBack = (i > 200) ? 200 : (int)i;
        for (int back = 0; back < searchBack; back++) {
            if (base[i - back] == (byte_t)configId) {
                hasIdRef = true;
                break;
            }
        }
        
        if (!hasIdRef) continue;
        
        DebugLog("[FALLBACK] Found target pattern at offset %p", (void*)(base + i - base));
        
        // Patch: reverse conditional jump
        if (jmpByte == 0x75) {
            return PatchByte(base + i + 2, 0x75, 0x74);
        } else if (jmpByte == 0x74) {
            return PatchByte(base + i + 2, 0x74, 0x75);
        }
    }
    
    DebugLog("[FALLBACK] Pattern not found in code section");
    return false;
}

// Patch: make IP_AllowWithoutAuth visible in release mode
static bool PatchConfigVisibility(byte_t* base, size_t size)
{
    DebugLog("[PATCH] Starting pattern search in %zu bytes...", size);
    
    // Define multiple patterns with priority (from specific to generic)
    PatternDef patterns[] = {
        // Pattern 1: Exact match (current version 2024)
        // TEST BL,BL + JNZ +0x11 + MOV RAX,[RAX+0x20]
        {
            { 0x84, 0xDB, 0x75, 0x11, 0x48, 0x8B, 0x40, 0x20 },
            "xxxxxxxx",
            8, 2, 0x75, 0x74,
            "Windows exact (TEST BL,BL + JNZ 0x11)"
        },
        // Pattern 2: Jump offset may vary
        {
            { 0x84, 0xDB, 0x75, 0x00, 0x48, 0x8B, 0x40, 0x20 },
            "xxx?xxxx",
            8, 2, 0x75, 0x74,
            "Windows flexible offset (TEST BL,BL + JNZ ?)"
        },
        // Pattern 3: Register may vary (BL→CL: 84 C9, BL→DL: 84 D2)
        {
            { 0x84, 0x00, 0x75, 0x00, 0x48, 0x8B, 0x40, 0x20 },
            "x?x?xxxx",
            8, 2, 0x75, 0x74,
            "Cross-platform (TEST ?,? + JNZ ?)"
        },
        // Pattern 4: Compiler may use JZ instead of JNZ (logic reversed)
        {
            { 0x84, 0xDB, 0x74, 0x00, 0x48, 0x8B, 0x40, 0x20 },
            "xxx?xxxx",
            8, 2, 0x74, 0x75,
            "Windows JZ variant (TEST BL,BL + JZ ?)"
        },
        // Pattern 5: Register + JZ
        {
            { 0x84, 0x00, 0x74, 0x00, 0x48, 0x8B, 0x40, 0x20 },
            "x?x?xxxx",
            8, 2, 0x74, 0x75,
            "Cross-platform JZ (TEST ?,? + JZ ?)"
        },
        // Pattern 6: Long jump form (0F 85 = JNZ near)
        {
            { 0x84, 0xDB, 0x0F, 0x85 },
            "xxxx",
            4, 3, 0x85, 0x84,
            "Long jump JNZ (TEST BL,BL + JNZ near)"
        },
        // Pattern 7: Long jump JZ form
        {
            { 0x84, 0xDB, 0x0F, 0x84 },
            "xxxx",
            4, 3, 0x84, 0x85,
            "Long jump JZ (TEST BL,BL + JZ near)"
        },
#ifndef __WINDOWS__
        // Pattern 8: Unix/Linux specific - TEST AL,AL variant
        {
            { 0x84, 0xC0, 0x75, 0x00, 0x48, 0x8B, 0x40, 0x20 },
            "xxx?xxxx",
            8, 2, 0x75, 0x74,
            "Unix (TEST AL,AL + JNZ ?)"
        },
        // Pattern 9: Unix with different register
        {
            { 0x84, 0xC9, 0x75, 0x00, 0x48, 0x8B, 0x40, 0x20 },
            "xxx?xxxx",
            8, 2, 0x75, 0x74,
            "Unix (TEST CL,CL + JNZ ?)"
        },
#endif
    };
    
    int numPatterns = sizeof(patterns) / sizeof(patterns[0]);
    
    // Try each pattern in order
    for (int i = 0; i < numPatterns; i++) {
        PatternDef& p = patterns[i];
        
        int count = CountPattern(base, size, p.pattern, p.mask, p.len);
        DebugLog("[PATCH] Pattern %d (%s): %d matches", i + 1, p.description, count);
        
        if (count == 1) {
            byte_t* addr = FindPattern(base, size, p.pattern, p.mask, p.len);
            if (!addr) continue;
            
            // Check if already patched
            if (*(addr + p.patchOffset) == p.newByte) {
                DebugLog("[PATCH] Already patched (byte is already 0x%02X)", p.newByte);
                return true;
            }
            
            DebugLog("[PATCH] Found unique match at offset %p", (void*)(addr - base));
            if (PatchByte(addr + p.patchOffset, p.oldByte, p.newByte)) {
                return true;
            } else {
                DebugLog("[PATCH] Patch failed for pattern %d", i + 1);
                return false;
            }
        } else if (count > 1) {
            DebugLog("[PATCH] Pattern %d has multiple matches, trying next pattern", i + 1);
        }
    }
    
    // All predefined patterns failed, try fallback
    DebugLog("[PATCH] All predefined patterns failed, trying fallback method");
    return FallbackPatch(base, size);
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

// ============================================================
// VAC Secure Flag Patch for client.dll
// ============================================================
// When BSecureAllowed() returns false (because steamnetworkingsockets.dll
// cannot reach Valve infrastructure in LAN mode), client.dll sets a global
// byte to 1 which later causes lobby invite accepts to be silently converted
// to declines (the "cannot verify this machine is secure" popup).
//
// We patch the instruction that writes 1 into that global byte so it writes 0
// instead, effectively neutralizing the check.
//
// Target instruction in client.dll:
//   test eax, eax          ; 85 C0
//   je   +0x26             ; 74 26
//   mov byte ptr [rip+X],1 ; C6 05 xx xx xx xx 01  <-- patch 01 -> 00
//   xor eax, eax           ; 33 C0
//   cmp eax, 1             ; 83 F8 01

#ifdef __WINDOWS__
static void* GetClientDllModule(byte_t** outBase, size_t* outSize)
{
    const char* dllNames[] = {
        "client.dll",
        nullptr
    };

    DebugLog("[VAC_PATCH] Searching for client.dll...");
    
    // Retry for 120 seconds (1200 attempts * 100ms)
    // client.dll loads late (when entering game UI)
    for (int retry = 0; retry < 1200; retry++) {
        for (int i = 0; dllNames[i]; i++) {
            HMODULE h = GetModuleHandleA(dllNames[i]);
            if (h) {
                MODULEINFO modInfo;
                if (GetModuleInformation(GetCurrentProcess(), h, &modInfo, sizeof(modInfo))) {
                    *outBase = (byte_t*)modInfo.lpBaseOfDll;
                    *outSize = modInfo.SizeOfImage;
                    DebugLog("[VAC_PATCH] Found %s at base %p, size %zu bytes", dllNames[i], *outBase, *outSize);
                    return h;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    DebugLog("[VAC_PATCH] Failed to find client.dll after 120 seconds");
    return nullptr;
}
#else
static void* GetClientDllModule(byte_t** outBase, size_t* outSize)
{
    const char* libNames[] = {
        "libclient.so",
        "client.so",
        nullptr
    };

    DebugLog("[VAC_PATCH] Searching for client library...");
    
    for (int retry = 0; retry < 1200; retry++) {
        for (int i = 0; libNames[i]; i++) {
            void* handle = dlopen(libNames[i], RTLD_LAZY | RTLD_NOLOAD);
            if (handle) {
#ifdef __APPLE__
                uint32_t imageCount = _dyld_image_count();
                for (uint32_t j = 0; j < imageCount; j++) {
                    const char* imageName = _dyld_get_image_name(j);
                    if (imageName && strstr(imageName, "client")) {
                        const struct mach_header* header = (const struct mach_header*)_dyld_get_image_header(j);
                        if (header) {
                            *outBase = (byte_t*)header;
                            *outSize = 100 * 1024 * 1024; // 100MB estimate
                            DebugLog("[VAC_PATCH] Found %s at base %p", imageName, *outBase);
                            return handle;
                        }
                    }
                }
#else
                struct link_map* map = nullptr;
                if (dlinfo(handle, RTLD_DI_LINKMAP, &map) == 0 && map) {
                    *outBase = (byte_t*)map->l_addr;
                    *outSize = 100 * 1024 * 1024; // 100MB estimate
                    DebugLog("[VAC_PATCH] Found %s at base %p", libNames[i], *outBase);
                    return handle;
                }
#endif
                dlclose(handle);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    DebugLog("[VAC_PATCH] Failed to find client library after 120 seconds");
    return nullptr;
}
#endif

// Patch the VAC secure flag write in client.dll
static bool PatchVACSecureFlag(byte_t* base, size_t size)
{
    DebugLog("[VAC_PATCH] Starting VAC secure flag patch in %zu bytes...", size);
    
    // Pattern: 85 C0 74 26 C6 05 ?? ?? ?? ?? 01 33 C0 83 F8 01
    // Meaning: test eax,eax / je +0x26 / mov byte ptr [rip+X], 1 / xor eax,eax / cmp eax,1
    // We patch the 01 (immediate value) to 00 so it writes 0 instead of 1
    
    // Pattern 1: Exact match with je offset 0x26
    byte_t pattern1[] = { 0x85, 0xC0, 0x74, 0x26, 0xC6, 0x05 };
    byte_t suffix1[] = { 0x01, 0x33, 0xC0, 0x83, 0xF8, 0x01 };
    
    for (size_t i = 0; i < size - 20; i++) {
        if (memcmp(base + i, pattern1, sizeof(pattern1)) != 0) continue;
        // Skip 4 bytes of RIP-relative offset
        if (memcmp(base + i + 10, suffix1, sizeof(suffix1)) != 0) continue;
        
        DebugLog("[VAC_PATCH] Found exact pattern at offset 0x%zX", i);
        // Patch byte at offset i+10 (the immediate 01 in mov byte ptr [rip+X], 1)
        return PatchByte(base + i + 10, 0x01, 0x00);
    }
    
    // Pattern 2: Flexible je offset (74 ??)
    byte_t pattern2_prefix[] = { 0x85, 0xC0, 0x74 };
    byte_t pattern2_mov[] = { 0xC6, 0x05 };
    byte_t suffix2[] = { 0x33, 0xC0, 0x83, 0xF8, 0x01 };
    
    for (size_t i = 0; i < size - 20; i++) {
        if (memcmp(base + i, pattern2_prefix, sizeof(pattern2_prefix)) != 0) continue;
        // base[i+3] is the je offset (variable)
        // base[i+4..i+5] should be C6 05 (mov byte ptr [rip+...])
        if (memcmp(base + i + 4, pattern2_mov, sizeof(pattern2_mov)) != 0) continue;
        // base[i+6..i+9] is the 4-byte RIP offset (skip)
        // base[i+10] should be 0x01 (the value being written)
        if (base[i + 10] != 0x01) continue;
        // base[i+11..i+15] should match suffix
        if (memcmp(base + i + 11, suffix2, sizeof(suffix2)) != 0) continue;
        
        DebugLog("[VAC_PATCH] Found flexible pattern at offset 0x%zX", i);
        return PatchByte(base + i + 10, 0x01, 0x00);
    }
    
    // Pattern 3: Long conditional jump (0F 84 instead of 74)
    // test eax,eax / je near [0F 84 xx xx xx xx] / mov byte ptr [rip+X], 1
    byte_t pattern3_prefix[] = { 0x85, 0xC0, 0x0F, 0x84 };
    
    for (size_t i = 0; i < size - 24; i++) {
        if (memcmp(base + i, pattern3_prefix, sizeof(pattern3_prefix)) != 0) continue;
        // Skip 4 bytes of jump offset: i+4..i+7
        // base[i+8..i+9] should be C6 05
        if (memcmp(base + i + 8, pattern2_mov, sizeof(pattern2_mov)) != 0) continue;
        // base[i+10..i+13] is RIP offset
        // base[i+14] should be 0x01
        if (base[i + 14] != 0x01) continue;
        // base[i+15..i+19] should match suffix
        if (memcmp(base + i + 15, suffix2, sizeof(suffix2)) != 0) continue;
        
        DebugLog("[VAC_PATCH] Found long-jump pattern at offset 0x%zX", i);
        return PatchByte(base + i + 14, 0x01, 0x00);
    }
    
    DebugLog("[VAC_PATCH] No matching pattern found");
    return false;
}

// Main entry point for VAC patch
static void ApplyVACPatch()
{
    DebugLog("[VAC_MAIN] VACPatch thread starting");
    
    std::thread([]() {
        DebugLog("[VAC_THREAD] VAC patch thread started");
        
        byte_t* base = nullptr;
        size_t size = 0;
        
        void* handle = GetClientDllModule(&base, &size);
        if (!handle || !base || size == 0) {
            DebugLog("[VAC_THREAD] Failed to get client.dll module information");
            return;
        }
        
        bool patched = PatchVACSecureFlag(base, size);
        
        if (patched) {
            DebugLog("[VAC_THREAD] VAC secure flag patch applied successfully!");
        } else {
            DebugLog("[VAC_THREAD] VAC secure flag patch failed - pattern not found");
        }
    }).detach();
}

} // namespace NetworkingPatch
