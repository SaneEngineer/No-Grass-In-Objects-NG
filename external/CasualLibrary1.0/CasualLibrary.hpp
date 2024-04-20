// Must be included solely by a user

#pragma once

#ifdef _WIN32 // Windows-only
// Memory
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <dbghelp.h> // ImageNtHeader
#include <cstdint>

#pragma comment(lib, "dbghelp.lib")

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6387)
#endif

//#if defined(_MSVC_LANG) && _MSVC_LANG >= 201703L || __cplusplus >= 201703L
#define CPP17GRT
//#endif

namespace Helper {
	[[nodiscard]] bool matchingBuilt(const HANDLE process) noexcept;
}

class Address
{
public:
    Address() noexcept = default;
    Address(const uintptr_t _ptr) noexcept : ptr(_ptr) {};
    Address(const void* _ptr) noexcept : ptr(reinterpret_cast<uintptr_t> (_ptr)) {};

    operator uintptr_t(void) const noexcept;
    explicit operator void* (void) const noexcept;

    bool isValid(void) const noexcept;

    template <typename T = uintptr_t>
    T get(void) const noexcept
    {
        return static_cast<T>(ptr);
    }

    Address addOffset(const uint32_t offset) noexcept;

protected:
    uintptr_t ptr;
};

namespace Memory {
	[[nodiscard]] std::vector<int> patternToBytes(const char* pattern) noexcept;
	[[nodiscard]] std::string getLastErrorAsString(void) noexcept;
    [[nodiscard]] std::string convertToString(char* a, int size) noexcept;

    // Sadly, only C++17 feature because of the folding
#ifdef CPP17GRT
    template<typename T, typename... Ts>
    [[nodiscard]] constexpr bool is_any_type(void) noexcept {
        return (std::is_same_v<T, Ts> || ...);
    }
#endif
}

namespace Memory {
    class External final
    {
    public:
        External(void) noexcept = default;
        External(const char* proc, bool debug = false) noexcept;
        ~External(void) noexcept;

        /*
        @brief uses loadlibrary from kernal32 to inject a dll into another process
        */
        bool DLLInject(const std::string& DllPath) {
            char szDllPath[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, szDllPath);
            strcat_s(szDllPath, DllPath.c_str());

            auto m_hProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, false, this->processID);

            auto pLoadLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
            if (!pLoadLibrary)
                return false;

            auto pAllocAddress = reinterpret_cast<std::uintptr_t>(VirtualAllocEx(m_hProcessHandle, 0, strlen(szDllPath) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            if (!pAllocAddress)
                return false;

            if (!WriteProcessMemory(m_hProcessHandle, reinterpret_cast<PVOID>(pAllocAddress), szDllPath, strlen(szDllPath) + 1, 0))
                return false;

            auto hThread = CreateRemoteThread(m_hProcessHandle, 0, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibrary), reinterpret_cast<LPVOID>(pAllocAddress), 0, 0);
            if (hThread == INVALID_HANDLE_VALUE)
                return false;

            WaitForSingleObject(hThread, INFINITE);
            VirtualFreeEx(m_hProcessHandle, reinterpret_cast<LPVOID>(pAllocAddress), 0, MEM_RELEASE);

            return true;
        }
        /**
        @brief reads a value from memory, supports strings in C++17 or greater.
        @param addToBeRead address from which the value will be read.
        @param memoryCheck flag if true memory protection will be checked/changed before reading.
        */
        template <typename T>
        [[nodiscard]] T read(const Address& addToBeRead, const bool memoryCheck = false) noexcept {
            const uintptr_t address = addToBeRead.get();

            if (memoryCheck) {
                MEMORY_BASIC_INFORMATION mbi;
                VirtualQueryEx(handle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi));

                if (!(mbi.State & MEM_COMMIT)) {
                    return T{};
                }

                if (mbi.Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_NOACCESS)) {
                    if (!VirtualProtectEx(handle, mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &mbi.Protect)) {
                        return T{};
                    }
#ifdef   CPP17GRT           	
                    if constexpr (is_any_type<T, const char*, std::string, char*>()) {
                        constexpr const std::size_t size = 200;
                        std::vector<char> chars(size);
                        if (!ReadProcessMemory(handle, reinterpret_cast<LPBYTE*>(address), chars.data(), size, NULL) && debug)
                            std::cout << getLastErrorAsString() << std::endl;

                        const std::string name(chars.begin(), chars.end());
                        DWORD dwOldProtect;
                        VirtualProtectEx(handle, mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &dwOldProtect);

                        return name.substr(0, name.find('\0'));;
                    }
#endif

                    T varBuff;
                    ReadProcessMemory(handle, reinterpret_cast<LPBYTE*>(address), &varBuff, sizeof(varBuff), nullptr);

                    DWORD dwOldProtect;
                    VirtualProtectEx(handle, mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &dwOldProtect);
                    return varBuff;
                }

            }

#ifdef CPP17GRT
            if constexpr (is_any_type<T, const char*, std::string, char*>()) {
                constexpr const std::size_t size = 200;
                std::vector<char> chars(size);
                if (!ReadProcessMemory(handle, reinterpret_cast<LPBYTE*>(address), chars.data(), size, NULL) && debug)
                    std::cout << getLastErrorAsString() << std::endl;

                const std::string name(chars.begin(), chars.end());

                return name.substr(0, name.find('\0'));;
            }
#endif

            T varBuff;
            ReadProcessMemory(handle, reinterpret_cast<LPBYTE*>(address), &varBuff, sizeof(varBuff), nullptr);
            return varBuff;
        }

        /**
        @brief writes a value to memory
        @param addToWrite address to which the value will be written to.
        @param valToWrite the value that gets written to desired address.
        @param memoryCheck flag if true memory protection will be checked/changed before writing.
        */
        template <typename T>
        T write(const uintptr_t addToWrite, const T valToWrite, const bool memoryCheck = false) noexcept {
            if (memoryCheck) {
                MEMORY_BASIC_INFORMATION mbi;
                VirtualQueryEx(handle, reinterpret_cast<LPCVOID>(addToWrite), &mbi, sizeof(mbi));

                if (!(mbi.State & MEM_COMMIT)) {
                    return T{};
                }

                if (mbi.Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_NOACCESS | PAGE_READONLY)) {
                    if (!VirtualProtectEx(handle, mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &mbi.Protect)) {
                        return T{};
                    }

                    WriteProcessMemory(handle, reinterpret_cast<LPBYTE*>(addToWrite), &valToWrite, sizeof(valToWrite), nullptr);

                    DWORD dwOldProtect;
                    VirtualProtectEx(handle, mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &dwOldProtect);
                    return valToWrite;
                }

            }

            WriteProcessMemory(handle, reinterpret_cast<LPBYTE*>(addToWrite), &valToWrite, sizeof(valToWrite), nullptr);
            return valToWrite;
        }

        [[deprecated("use read function with std::string/const char* template argument")]] [[nodiscard]] std::string readString(const uintptr_t addToBeRead, std::size_t size = 0) noexcept;

        /**@brief returns process id of the target process*/
        [[nodiscard]] DWORD getProcessID(void) noexcept;

        /**@brief returns the module base address of \p modName (example: "client.dll")*/
        [[nodiscard]] Address getModule(const char* modName) noexcept;

        /**@brief returns the dynamic pointer from base pointer \p basePrt and \p offsets*/
        [[nodiscard]] Address getAddress(const uintptr_t basePrt, const std::vector<uintptr_t>& offsets) noexcept;

        /**@brief returns the dynamic pointer from base pointer \p basePrt and \p offsets*/
        [[nodiscard]] Address getAddress(const Address& basePrt, const std::vector<uintptr_t>& offsets) noexcept;

        [[nodiscard]] bool memoryCompare(const BYTE* bData, const std::vector<int>& signature) noexcept;

        /**
        @brief a basic signature scanner
        @param start Address where to start scanning.
        @param sig Signature to search, for example: "? 39 05 F0 A2 F6 FF" where "?" (-1) is a wildcard.
        @param size Size of the area to search within.
        */
        [[nodiscard]] Address findSignature(const uintptr_t start, const char* sig, const size_t size) noexcept;
        [[nodiscard]] Address findSignature(const Address& start, const char* sig, const size_t size) noexcept;
    private:
        /** @brief handle of the target process */
        HANDLE handle = nullptr;

        /** @brief ID of the target process */
        DWORD processID = 0;

        /** @brief debug flag. Set true for debug messages*/
        bool debug = false;

        /**@brief initialize member variables.
        @param proc Name of the target process ("target.exe")
        @param access Desired access right to the process
        @returns whether or not initialization was successful*/
        bool init(const char* proc, const DWORD access = PROCESS_ALL_ACCESS) noexcept;

    };
}

namespace Memory {
    namespace Internal
    {
        /**
        @brief reads a value from memory
        @param addToBeRead address from which the value will be read.
        @param memoryCheck flag if true memory protection will be checked/changed before reading.
        */
        template <typename T>
        [[nodiscard]] T read(const Address& addToBeRead, const bool memoryCheck = false) noexcept {
            const uintptr_t address = addToBeRead.get();

            if (memoryCheck) {
                MEMORY_BASIC_INFORMATION mbi;
                VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi));

                if (!(mbi.State & MEM_COMMIT)) {
                    return T{};
                }

                if (mbi.Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_NOACCESS)) {
                    if (!VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &mbi.Protect)) {
                        return T{};
                    }

#ifdef CPP17GRT
                    if constexpr (is_any_type<T, const char*, std::string, char*>()) {
                        constexpr const std::size_t size = 200;
                        char chars[size] = "";
                        memcpy(chars, reinterpret_cast<char*>(address), size);

                        int sizeString = sizeof(chars) / sizeof(char);
                        const std::string name = convertToString(chars, sizeString);

                        DWORD dwOldProtect;
                        VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &dwOldProtect);
                        return name.substr(0, name.find('\0'));;
                    }
#endif

                    T returnValue = *reinterpret_cast<T*>(address);

                    DWORD dwOldProtect;
                    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &dwOldProtect);
                    return returnValue;
                }
            }

#ifdef CPP17GRT
            if constexpr (is_any_type<T, const char*, std::string, char*>()) {
                constexpr const std::size_t size = 200;
                char chars[size] = "";
                memcpy(chars,reinterpret_cast<char*>(address),size);

                int sizeString = sizeof(chars) / sizeof(char);
                const std::string name = convertToString(chars, sizeString);

                return name.substr(0, name.find('\0'));;
            }
#endif

            return *reinterpret_cast<T*>(address);
        }

        /**
        @brief writes a value to memory
        @param address address to which the value will be written to.
        @param value the value that gets written to desired address.
        @param memoryCheck flag if true memory protection will be checked/changed before writing.
        */
        template<typename T>
        void write(const uintptr_t address, const T value, const bool memoryCheck = false) noexcept {
            try {
                if (memoryCheck) {
                    MEMORY_BASIC_INFORMATION mbi;
                    VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi));

                    if (!(mbi.State & MEM_COMMIT)) {
                        return;
                    }

                    if (mbi.Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_NOACCESS | PAGE_READONLY | PAGE_EXECUTE_READ)) {
						if (!(mbi.Protect & PAGE_EXECUTE_READ)) {
                            if (!VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &mbi.Protect)) {
                                return;
                            }
					    }	else {
							if (!VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect)) {
								return;
							}
					    }
                        *reinterpret_cast<T*>(address) = value;

                        DWORD dwOldProtect;
                        VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &dwOldProtect);
                        return;
                    }
                }
                *reinterpret_cast<T*>(address) = value;
            } catch (...) {
                return;
            }
        }

        /**@brief returns the module base address of \p modName (example: "client.dll")*/
        [[nodiscard]] Address getModule(const char* modName) noexcept;

        /**@brief returns the dynamic pointer from base pointer \p basePrt and \p offsets*/
        [[nodiscard]] Address getAddress(const uintptr_t basePrt, const std::vector<uintptr_t>& offsets) noexcept;

        /**@brief returns the dynamic pointer from base pointer \p basePrt and \p offsets*/
        [[nodiscard]] Address getAddress(const Address& basePrt, const std::vector<uintptr_t>& offsets) noexcept;

        /**
        @brief a basic signature scanner
        @param start Address where to start scanning.
        @param sig Signature to search, for example: "? 39 05 F0 A2 F6 FF" where "?" (-1) is a wildcard.
        @param size Size of the area to search within.
        */
        [[nodiscard]] Address findSignature(const uintptr_t start, const char* sig, const size_t size = 0) noexcept;
        [[nodiscard]] Address findSignature(const Address& modAddr, const char* sig, const size_t size = 0) noexcept;

        /**
        DO NOT IGNORE: YOU ALWAYS HAVE TO USE THIS FUNCTION (findModuleSignature) WHEN SCANNING FOR SIGNATURES,
        BECAUSE IT RECEIVES THE MODULE BASE AND SIZE OF IMAGE, YOU SHOULD THINK OF IT AS A WRAPPER FOR THE findSignature FUNCTION.
        @brief a basic signature scanner searching within the address space of one module.
        @param mod name of the module ("client.dll")
        @param sig Signature to search, for example: "? 39 05 F0 A2 F6 FF" where "?" (-1) is a wildcard.
        */
        [[nodiscard]] Address findModuleSignature(const char* mod, const char* sig) noexcept;

        /**
         @brief gets a virtual function via vtable index, don't use this, use callVirtualFunction,
         the only reason this is here is because its wrapped around callVirtualFunction.
         @param baseClass vtable type
         @param index index in vtable
         */
        template<typename T>
        [[nodiscard]] T getVirtualFunction(const void* baseClass, const uint32_t index) noexcept {
            return (*static_cast<T**>(baseClass))[index];
        }

        /**
        @brief calls a virtual function
        @param baseClass vtable type
        @param index index in vtable
        @param args the function's parameters
        */
        template<typename T, typename... Args>
        T callVirtualFunction(const void* baseClass, const uint32_t index, Args&&... args) noexcept {
            return getVirtualFunction<T(__thiscall*)(void*, Args&&...)>(baseClass, index)(baseClass, std::forward<Args>(args)...);
        }

    }
}

#endif
