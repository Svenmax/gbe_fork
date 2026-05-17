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

#ifdef __WINDOWS__

#include <windows.h>
#include <psapi.h>
#include <cstring>
#include <thread>
#include <chrono>

namespace NetworkingPatch {

// 在内存中搜索字节特征（支持通配符）
static BYTE* FindPattern(BYTE* base, DWORD size, const BYTE* pattern, const char* mask, int patternLen)
{
    for (DWORD i = 0; i < size - patternLen; i++) {
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

// 修改内存中的字节
static bool PatchByte(BYTE* address, BYTE oldValue, BYTE newValue)
{
    if (*address != oldValue) return false;

    DWORD oldProtect;
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    *address = newValue;
    VirtualProtect(address, 1, oldProtect, &oldProtect);
    return true;
}

// 等待并获取 steamnetworkingsockets.dll 模块句柄
static HMODULE GetSteamNetworkingSocketsModule()
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
            if (h) return h;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}

// 补丁：让 IP_AllowWithoutAuth 参数在 release 模式下可见
// 特征：TEST BL,BL + JNZ +0x11 + MOV RAX,[RAX+0x20]
// 字节：84 DB 75 11 48 8B 40 20
// 修改：75 -> 74 (JNZ -> JZ)
static bool PatchConfigVisibility(BYTE* base, DWORD size)
{
    BYTE pattern[] = { 0x84, 0xDB, 0x75, 0x11, 0x48, 0x8B, 0x40, 0x20 };
    char mask[] = "xxxxxxxx";
    int patternLen = 8;

    BYTE* addr = FindPattern(base, size, pattern, mask, patternLen);
    if (addr) {
        return PatchByte(addr + 2, 0x75, 0x74); // JNZ -> JZ
    }
    return false;
}

// 主入口：应用所有补丁
static void ApplyAll()
{
    // 在后台线程中执行，避免阻塞 DLL 加载
    std::thread([]() {
        HMODULE hModule = GetSteamNetworkingSocketsModule();
        if (!hModule) {
            // 未找到 steamnetworkingsockets.dll，可能游戏不使用它
            return;
        }

        MODULEINFO modInfo;
        if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) {
            return;
        }

        BYTE* base = (BYTE*)modInfo.lpBaseOfDll;
        DWORD size = modInfo.SizeOfImage;

        // 应用补丁：让 IP_AllowWithoutAuth 参数可见
        bool patched = PatchConfigVisibility(base, size);
        
        // 可选：记录补丁结果到日志
        // 注意：这里不使用 PRINT_DEBUG，因为可能在后台线程中
        if (patched) {
            // 补丁成功
        } else {
            // 补丁失败或未找到特征字节
            // 可能是 DLL 版本不匹配
        }
    }).detach();
}

} // namespace NetworkingPatch

#endif // __WINDOWS__
