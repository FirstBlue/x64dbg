#include "cmd-analysis.h"
#include "linearanalysis.h"
#include "memory.h"
#include "exceptiondirectoryanalysis.h"
#include "controlflowanalysis.h"
#include "analysis_nukem.h"
#include "xrefsanalysis.h"
#include "recursiveanalysis.h"
#include "value.h"
#include "advancedanalysis.h"
#include "debugger.h"
#include "variable.h"
#include "exhandlerinfo.h"
#include "symbolinfo.h"
#include "exception.h"
#include "TraceRecord.h"

CMDRESULT cbInstrAnalyse(int argc, char* argv[])
{
    SELECTIONDATA sel;
    GuiSelectionGet(GUI_DISASSEMBLY, &sel);
    duint size = 0;
    duint base = MemFindBaseAddr(sel.start, &size);
    LinearAnalysis anal(base, size);
    anal.Analyse();
    anal.SetMarkers();
    GuiUpdateAllViews();
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrExanalyse(int argc, char* argv[])
{
    SELECTIONDATA sel;
    GuiSelectionGet(GUI_DISASSEMBLY, &sel);
    duint size = 0;
    duint base = MemFindBaseAddr(sel.start, &size);
    ExceptionDirectoryAnalysis anal(base, size);
    anal.Analyse();
    anal.SetMarkers();
    GuiUpdateAllViews();
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrCfanalyse(int argc, char* argv[])
{
    bool exceptionDirectory = false;
    if(argc > 1)
        exceptionDirectory = true;
    SELECTIONDATA sel;
    GuiSelectionGet(GUI_DISASSEMBLY, &sel);
    duint size = 0;
    duint base = MemFindBaseAddr(sel.start, &size);
    ControlFlowAnalysis anal(base, size, exceptionDirectory);
    anal.Analyse();
    anal.SetMarkers();
    GuiUpdateAllViews();
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrAnalyseNukem(int argc, char* argv[])
{
    SELECTIONDATA sel;
    GuiSelectionGet(GUI_DISASSEMBLY, &sel);
    duint size = 0;
    duint base = MemFindBaseAddr(sel.start, &size);
    Analyse_nukem(base, size);
    GuiUpdateAllViews();
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrAnalxrefs(int argc, char* argv[])
{
    SELECTIONDATA sel;
    GuiSelectionGet(GUI_DISASSEMBLY, &sel);
    duint size = 0;
    auto base = MemFindBaseAddr(sel.start, &size);
    XrefsAnalysis anal(base, size);
    anal.Analyse();
    anal.SetMarkers();
    GuiUpdateAllViews();
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrAnalrecur(int argc, char* argv[])
{
    if(IsArgumentsLessThan(argc, 2))
        return STATUS_ERROR;
    duint entry;
    if(!valfromstring(argv[1], &entry, false))
        return STATUS_ERROR;
    duint size;
    auto base = MemFindBaseAddr(entry, &size);
    if(!base)
        return STATUS_ERROR;
    RecursiveAnalysis analysis(base, size, entry, 0);
    analysis.Analyse();
    analysis.SetMarkers();
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrAnalyseadv(int argc, char* argv[])
{
    SELECTIONDATA sel;
    GuiSelectionGet(GUI_DISASSEMBLY, &sel);
    duint size = 0;
    auto base = MemFindBaseAddr(sel.start, &size);
    AdvancedAnalysis anal(base, size);
    anal.Analyse();
    anal.SetMarkers();
    GuiUpdateAllViews();
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrVirtualmod(int argc, char* argv[])
{
    if(IsArgumentsLessThan(argc, 3))
        return STATUS_ERROR;
    duint base;
    if(!valfromstring(argv[2], &base))
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "Invalid parameter [base]!"));
        return STATUS_ERROR;
    }
    if(!MemIsValidReadPtr(base))
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "Invalid memory address!"));
        return STATUS_ERROR;
    }
    duint size;
    if(argc < 4)
        base = MemFindBaseAddr(base, &size);
    else if(!valfromstring(argv[3], &size))
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "Invalid parameter [size]"));
        return STATUS_ERROR;
    }
    auto name = String("virtual:\\") + (argv[1]);
    if(!ModLoad(base, size, name.c_str()))
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "Failed to load module (ModLoad)..."));
        return STATUS_ERROR;
    }

    char modname[256] = "";
    if(ModNameFromAddr(base, modname, true))
        BpEnumAll(cbSetModuleBreakpoints, modname);

    dprintf(QT_TRANSLATE_NOOP("DBG", "Virtual module \"%s\" loaded on %p[%p]!\n"), argv[1], base, size);
    GuiUpdateAllViews();
    return STATUS_CONTINUE;
}

CMDRESULT cbDebugDownloadSymbol(int argc, char* argv[])
{
    dputs(QT_TRANSLATE_NOOP("DBG", "This may take very long, depending on your network connection and data in the debug directory..."));
    char szDefaultStore[MAX_SETTING_SIZE] = "";
    const char* szSymbolStore = szDefaultStore;
    if(!BridgeSettingGet("Symbols", "DefaultStore", szDefaultStore))  //get default symbol store from settings
    {
        strcpy_s(szDefaultStore, "http://msdl.microsoft.com/download/symbols");
        BridgeSettingSet("Symbols", "DefaultStore", szDefaultStore);
    }
    if(argc < 2)  //no arguments
    {
        SymDownloadAllSymbols(szSymbolStore); //download symbols for all modules
        GuiSymbolRefreshCurrent();
        dputs(QT_TRANSLATE_NOOP("DBG", "Done! See symbol log for more information"));
        return STATUS_CONTINUE;
    }
    //get some module information
    duint modbase = ModBaseFromName(argv[1]);
    if(!modbase)
    {
        dprintf(QT_TRANSLATE_NOOP("DBG", "Invalid module \"%s\"!\n"), argv[1]);
        return STATUS_ERROR;
    }
    wchar_t wszModulePath[MAX_PATH] = L"";
    if(!GetModuleFileNameExW(fdProcessInfo->hProcess, (HMODULE)modbase, wszModulePath, MAX_PATH))
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "GetModuleFileNameExA failed!"));
        return STATUS_ERROR;
    }
    wchar_t szOldSearchPath[MAX_PATH] = L"";
    if(!SafeSymGetSearchPathW(fdProcessInfo->hProcess, szOldSearchPath, MAX_PATH))  //backup current search path
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "SymGetSearchPath failed!"));
        return STATUS_ERROR;
    }
    char szServerSearchPath[MAX_PATH * 2] = "";
    if(argc > 2)
        szSymbolStore = argv[2];
    sprintf_s(szServerSearchPath, "SRV*%s*%s", szSymbolCachePath, szSymbolStore);
    if(!SafeSymSetSearchPathW(fdProcessInfo->hProcess, StringUtils::Utf8ToUtf16(szServerSearchPath).c_str()))  //set new search path
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "SymSetSearchPath (1) failed!"));
        return STATUS_ERROR;
    }
    if(!SafeSymUnloadModule64(fdProcessInfo->hProcess, (DWORD64)modbase))  //unload module
    {
        SafeSymSetSearchPathW(fdProcessInfo->hProcess, szOldSearchPath);
        dputs(QT_TRANSLATE_NOOP("DBG", "SymUnloadModule64 failed!"));
        return STATUS_ERROR;
    }
    auto symOptions = SafeSymGetOptions();
    SafeSymSetOptions(symOptions & ~SYMOPT_IGNORE_CVREC);
    if(!SymLoadModuleExW(fdProcessInfo->hProcess, 0, wszModulePath, 0, (DWORD64)modbase, 0, 0, 0))  //load module
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "SymLoadModuleEx failed!"));
        SafeSymSetOptions(symOptions);
        SafeSymSetSearchPathW(fdProcessInfo->hProcess, szOldSearchPath);
        return STATUS_ERROR;
    }
    SafeSymSetOptions(symOptions);
    if(!SafeSymSetSearchPathW(fdProcessInfo->hProcess, szOldSearchPath))
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "SymSetSearchPathW (2) failed!"));
        return STATUS_ERROR;
    }
    GuiSymbolRefreshCurrent();
    dputs(QT_TRANSLATE_NOOP("DBG", "Done! See symbol log for more information"));
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrImageinfo(int argc, char* argv[])
{
    duint mod;
    if(argc < 2 || !valfromstring(argv[1], &mod) || !ModBaseFromAddr(mod))
    {
        dputs(QT_TRANSLATE_NOOP("DBG", "invalid argument"));
        return STATUS_ERROR;
    }

    SHARED_ACQUIRE(LockModules);
    auto info = ModInfoFromAddr(mod);
    auto c = GetPE32DataFromMappedFile(info->fileMapVA, 0, UE_CHARACTERISTICS);
    auto dllc = GetPE32DataFromMappedFile(info->fileMapVA, 0, UE_DLLCHARACTERISTICS);
    SHARED_RELEASE();

    auto pFlag = [](ULONG_PTR value, ULONG_PTR flag, const char* name)
    {
        if((value & flag) == flag)
        {
            dprintf("  ");
            dputs(name);
        }
    };

    char modname[MAX_MODULE_SIZE] = "";
    ModNameFromAddr(mod, modname, true);

    dputs_untranslated("---------------");

    dprintf(QT_TRANSLATE_NOOP("DBG", "Image information for %s\n"), modname);

    dprintf(QT_TRANSLATE_NOOP("DBG", "Characteristics (0x%X):\n"), c);
    if(!c)
        dputs(QT_TRANSLATE_NOOP("DBG", "  None\n"));
    pFlag(c, IMAGE_FILE_RELOCS_STRIPPED, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_RELOCS_STRIPPED: Relocation info stripped from file."));
    pFlag(c, IMAGE_FILE_EXECUTABLE_IMAGE, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_EXECUTABLE_IMAGE: File is executable (i.e. no unresolved externel references)."));
    pFlag(c, IMAGE_FILE_LINE_NUMS_STRIPPED, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_LINE_NUMS_STRIPPED: Line numbers stripped from file."));
    pFlag(c, IMAGE_FILE_LOCAL_SYMS_STRIPPED, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_LOCAL_SYMS_STRIPPED: Local symbols stripped from file."));
    pFlag(c, IMAGE_FILE_AGGRESIVE_WS_TRIM, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_AGGRESIVE_WS_TRIM: Agressively trim working set"));
    pFlag(c, IMAGE_FILE_LARGE_ADDRESS_AWARE, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_LARGE_ADDRESS_AWARE: App can handle >2gb addresses"));
    pFlag(c, IMAGE_FILE_BYTES_REVERSED_LO, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_BYTES_REVERSED_LO: Bytes of machine word are reversed."));
    pFlag(c, IMAGE_FILE_32BIT_MACHINE, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_32BIT_MACHINE: 32 bit word machine."));
    pFlag(c, IMAGE_FILE_DEBUG_STRIPPED, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_DEBUG_STRIPPED: Debugging info stripped from file in .DBG file"));
    pFlag(c, IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP: If Image is on removable media, copy and run from the swap file."));
    pFlag(c, IMAGE_FILE_NET_RUN_FROM_SWAP, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_NET_RUN_FROM_SWAP: If Image is on Net, copy and run from the swap file."));
    pFlag(c, IMAGE_FILE_SYSTEM, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_SYSTEM: System File."));
    pFlag(c, IMAGE_FILE_DLL, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_DLL: File is a DLL."));
    pFlag(c, IMAGE_FILE_UP_SYSTEM_ONLY, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_UP_SYSTEM_ONLY: File should only be run on a UP machine"));
    pFlag(c, IMAGE_FILE_BYTES_REVERSED_HI, QT_TRANSLATE_NOOP("DBG", "IMAGE_FILE_BYTES_REVERSED_HI: Bytes of machine word are reversed."));

    dprintf(QT_TRANSLATE_NOOP("DBG", "DLL Characteristics (0x%X):\n"), dllc);
    if(!dllc)
        dputs(QT_TRANSLATE_NOOP("DBG", "  None\n"));
    pFlag(dllc, IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE, QT_TRANSLATE_NOOP("DBG", "IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE: DLL can move."));
    pFlag(dllc, IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY, QT_TRANSLATE_NOOP("DBG", "IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY: Code Integrity Image"));
    pFlag(dllc, IMAGE_DLLCHARACTERISTICS_NX_COMPAT, QT_TRANSLATE_NOOP("DBG", "IMAGE_DLLCHARACTERISTICS_NX_COMPAT: Image is NX compatible"));
    pFlag(dllc, IMAGE_DLLCHARACTERISTICS_NO_ISOLATION, QT_TRANSLATE_NOOP("DBG", "IMAGE_DLLCHARACTERISTICS_NO_ISOLATION: Image understands isolation and doesn't want it"));
    pFlag(dllc, IMAGE_DLLCHARACTERISTICS_NO_SEH, QT_TRANSLATE_NOOP("DBG", "IMAGE_DLLCHARACTERISTICS_NO_SEH: Image does not use SEH. No SE handler may reside in this image"));
    pFlag(dllc, IMAGE_DLLCHARACTERISTICS_NO_BIND, QT_TRANSLATE_NOOP("DBG", "IMAGE_DLLCHARACTERISTICS_NO_BIND: Do not bind this image."));
    pFlag(dllc, IMAGE_DLLCHARACTERISTICS_WDM_DRIVER, QT_TRANSLATE_NOOP("DBG", "IMAGE_DLLCHARACTERISTICS_WDM_DRIVER: Driver uses WDM model."));
    pFlag(dllc, IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE, QT_TRANSLATE_NOOP("DBG", "IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE: Remote Desktop Services aware."));

    dputs_untranslated("---------------");

    return STATUS_CONTINUE;
}

CMDRESULT cbInstrGetRelocSize(int argc, char* argv[])
{
    //Original tool "GetRelocSize" by Killboy/SND
    if(argc < 2)
    {
        _plugin_logputs(QT_TRANSLATE_NOOP("DBG", "Not enough arguments!"));
        return STATUS_ERROR;
    }

    duint RelocDirAddr;
    if(!valfromstring(argv[1], &RelocDirAddr, false))
        return STATUS_ERROR;

    duint RelocSize = 0;
    varset("$result", RelocSize, false);
    IMAGE_RELOCATION RelocDir;
    do
    {
        if(!MemRead(RelocDirAddr, &RelocDir, sizeof(IMAGE_RELOCATION)))
        {
            _plugin_logputs(QT_TRANSLATE_NOOP("DBG", "Invalid relocation table!"));
            return STATUS_ERROR;
        }
        if(!RelocDir.SymbolTableIndex)
            break;
        RelocSize += RelocDir.SymbolTableIndex;
        RelocDirAddr += RelocDir.SymbolTableIndex;
    }
    while(RelocDir.VirtualAddress);

    if(!RelocSize)
    {
        _plugin_logputs(QT_TRANSLATE_NOOP("DBG", "Invalid relocation table!"));
        return STATUS_ERROR;
    }

    varset("$result", RelocSize, false);
    _plugin_logprintf(QT_TRANSLATE_NOOP("DBG", "Relocation table size: %X\n"), RelocSize);

    return STATUS_CONTINUE;
}

static void printExhandlers(const char* name, const std::vector<duint> & entries)
{
    if(!entries.size())
        return;
    dprintf("%s:\n", name);
    for(auto entry : entries)
    {
        auto symbolic = SymGetSymbolicName(entry);
        if(symbolic.length())
            dprintf("%p %s\n", entry, symbolic.c_str());
        else
            dprintf("%p\n", entry);
    }
}

CMDRESULT cbInstrExhandlers(int argc, char* argv[])
{
    std::vector<duint> entries;
#ifndef _WIN64
    if(ExHandlerGetInfo(EX_HANDLER_SEH, entries))
    {
        std::vector<duint> handlers;
        for(auto entry : entries)
        {
            duint handler;
            if(MemRead(entry + sizeof(duint), &handler, sizeof(handler)))
                handlers.push_back(handler);
        }
        printExhandlers("StructuredExceptionHandler (SEH)", handlers);
    }
    else
        dputs(QT_TRANSLATE_NOOP("DBG", "Failed to get SEH (disabled?)"));
#endif //_WIN64

    if(ExHandlerGetInfo(EX_HANDLER_VEH, entries))
        printExhandlers("VectoredExceptionHandler (VEH)", entries);
    else
        dputs(QT_TRANSLATE_NOOP("DBG", "Failed to get VEH (loaded symbols for ntdll.dll?)"));

    if(ExHandlerGetInfo(EX_HANDLER_VCH, entries))
        printExhandlers("VectoredContinueHandler (VCH)", entries);
    else
        dputs(QT_TRANSLATE_NOOP("DBG", "Failed to get VCH (loaded symbols for ntdll.dll?)"));

    if(ExHandlerGetInfo(EX_HANDLER_UNHANDLED, entries))
        printExhandlers("UnhandledExceptionFilter", entries);
    else
        dputs(QT_TRANSLATE_NOOP("DBG", "Failed to get UnhandledExceptionFilter (loaded symbols for kernelbase.dll?)"));
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrExinfo(int argc, char* argv[])
{
    auto info = getLastExceptionInfo();
    const auto & record = info.ExceptionRecord;
    dputs("EXCEPTION_DEBUG_INFO:");
    dprintf("           dwFirstChance: %X\n", info.dwFirstChance);
    auto exceptionName = ExceptionCodeToName(record.ExceptionCode);
    if(!exceptionName.size())    //if no exception was found, try the error codes (RPC_S_*)
        exceptionName = ErrorCodeToName(record.ExceptionCode);
    if(exceptionName.size())
        dprintf("           ExceptionCode: %08X (%s)\n", record.ExceptionCode, exceptionName.c_str());
    else
        dprintf("           ExceptionCode: %08X\n", record.ExceptionCode);
    dprintf("          ExceptionFlags: %08X\n", record.ExceptionFlags);
    auto symbolic = SymGetSymbolicName(duint(record.ExceptionAddress));
    if(symbolic.length())
        dprintf("        ExceptionAddress: %p %s\n", record.ExceptionAddress, symbolic.c_str());
    else
        dprintf("        ExceptionAddress: %p\n", record.ExceptionAddress);
    dprintf("        NumberParameters: %d\n", record.NumberParameters);
    if(record.NumberParameters)
        for(DWORD i = 0; i < record.NumberParameters; i++)
        {
            symbolic = SymGetSymbolicName(duint(record.ExceptionInformation[i]));
            if(symbolic.length())
                dprintf("ExceptionInformation[%02d]: %p %s\n", i, record.ExceptionInformation[i], symbolic.c_str());
            else
                dprintf("ExceptionInformation[%02d]: %p\n", i, record.ExceptionInformation[i]);
        }
    return STATUS_CONTINUE;
}

CMDRESULT cbInstrTraceexecute(int argc, char* argv[])
{
    if(IsArgumentsLessThan(argc, 2))
        return STATUS_ERROR;
    duint addr;
    if(!valfromstring(argv[1], &addr, false))
        return STATUS_ERROR;
    _dbg_dbgtraceexecute(addr);
    return STATUS_CONTINUE;
}