#include "CPU.h"

#include "../modules/terminal.h"

#define EDX_SYSCALL                     (1 << 11)   // SYSCALL/SYSRET
#define EDX_XD                          (1 << 20)   // Execute Disable Bit
#define EDX_1GB_PAGE                    (1 << 26)   // 1 GB Pages
#define EDX_RDTSCP                      (1 << 27)   // RDTSCP and IA32_TSC_AUX
#define EDX_64_BIT                      (1 << 29)   // 64-bit Architecture


struct processor_info {
    uint32_t bit;
    const char* name;
};


static const struct processor_info features[29] = {
    /* {BIT, FEATURE NAME} |  DEFINITION     | DESCRIPTION                                   */
    /*---------------------|-----------------|---------------------------------------------- */
    {(1 << 0), "FPU"},     /* EDX_FPU        | Floating-Point Unit On-Chip                   */
    {(1 << 1), "VME"},     /* EDX_VME        | Virtual 8086 Mode Extensions                  */
    {(1 << 2), "DE"},      /* EDX_DE         | Debugging Extensions                          */
    {(1 << 3), "PSE"},     /* EDX_PSE        | Page Size Extension                           */
    {(1 << 4), "TSC"},     /* EDX_TSC        | Time Stamp Counter                            */
    {(1 << 5), "MSR"},     /* EDX_MSR        | Model Specific Registers                      */
    {(1 << 6), "PAE"},     /* EDX_PAE        | Physical Address Extension                    */
    {(1 << 7), "MCE"},     /* EDX_MCE        | Machine-Check Exception                       */
    {(1 << 8), "CX8"},     /* EDX_CX8        | CMPXCHG8 Instruction                          */
    {(1 << 9), "APIC"},    /* EDX_APIC       | APIC On-Chip                                  */
    {(1 << 11), "SEP"},    /* EDX_SEP        | SYSENTER/SYSEXIT instructions                 */
    {(1 << 12), "MTRR"},   /* EDX_MTRR       | Memory Type Range Registers                   */
    {(1 << 13), "PGE"},    /* EDX_PGE        | Page Global Bit                               */
    {(1 << 14), "MCA"},    /* EDX_MCA        | Machine-Check Architecture                    */
    {(1 << 15), "CMOV"},   /* EDX_CMOV       | Conditional Move Instruction                  */
    {(1 << 16), "PAT"},    /* EDX_PAT        | Page Attribute Table                          */
    {(1 << 17), "PSE36"},  /* EDX_PSE36      | 36-bit Page Size Extension                    */
    {(1 << 18), "PSN"},    /* EDX_PSN        | Processor Serial Number                       */
    {(1 << 19), "CLFLUSH"},/* EDX_CLFLUSH    | CLFLUSH Instruction                           */
    {(1 << 21), "DS"},     /* EDX_DS         | Debug Store                                   */
    {(1 << 22), "ACPI"},   /* EDX_ACPI       | Thermal Monitor and Software Clock Facilities */
    {(1 << 23), "MMX"},    /* EDX_MMX        | MMX Technology                                */
    {(1 << 24), "FXSR"},   /* EDX_FXSR       | FXSAVE and FXSTOR Instructions                */
    {(1 << 25), "SSE"},    /* EDX_SSE        | Streaming SIMD Extensions                     */
    {(1 << 26), "SSE2"},   /* EDX_SSE2       | Streaming SIMD Extensions 2                   */
    {(1 << 27), "SS"},     /* EDX_SS         | Self Snoop                                    */
    {(1 << 28), "HTT"},    /* EDX_HTT        | Multi-Threading                               */
    {(1 << 29), "TM"},     /* EDX_TM         | Thermal Monitor                               */

    {(0x80000000), "PBE"}  /* EDX_PBE        | Pending Break Enable                          */
};


static const struct processor_info instructions[29] = {
    /* {BIT, INSTRUCTION}   |  DEFINITION     | DESCRIPTION                                  */
    /*----------------------|-----------------|--------------------------------------------- */
    {(1 << 0), "SSE3"},     /* ECX_SSE3       | Streaming SIMD Extensions 3                  */
    {(1 << 1), "PCLMULQDQ"},/* ECX_PCLMULQDQ  | PCLMULQDQ Instruction                        */
    {(1 << 2), "DTES64"},   /* ECX_DTES64     | 64-Bit Debug Store Area                      */
    {(1 << 3), "MONITOR"},  /* ECX_MONITOR    | MONITOR/MWAIT                                */
    {(1 << 4), "DS_CPL"},   /* ECX_DS_CPL     | CPL Qualified Debug Store                    */
    {(1 << 5), "VMX"},      /* ECX_VMX        | Virtual Machine Extensions                   */
    {(1 << 6), "SMX"},      /* ECX_SMX        | Safer Mode Extensions                        */
    {(1 << 7), "EST"},      /* ECX_EST        | Enhanced SpeedStep Technology                */
    {(1 << 8), "TM2"},      /* ECX_TM2        | Thermal Monitor 2                            */
    {(1 << 9), "SSSE3"},    /* ECX_SSSE3      | Supplemental Streaming SIMD Extensions 3     */
    {(1 << 10), "CNXT_ID"}, /* ECX_CNXT_ID    | L1 Context ID                                */
    {(1 << 12), "FMA"},     /* ECX_FMA        | Fused Multiply Add                           */
    {(1 << 13), "CX16"},    /* ECX_CX16       | CMPXCHG16B Instruction                       */
    {(1 << 14), "XTPR"},    /* ECX_XTPR       | xTPR Update Control                          */
    {(1 << 15), "PDCM"},    /* ECX_PDCM       | Perf/Debug Capability MSR                    */
    {(1 << 17), "PCID"},    /* ECX_PCID       | Process-context Identifiers                  */
    {(1 << 18), "DCA"},     /* ECX_DCA        | Direct Cache Access                          */
    {(1 << 19), "SSE41"},   /* ECX_SSE41      | Streaming SIMD Extensions 4.1                */
    {(1 << 20), "SSE42"},   /* ECX_SSE42      | Streaming SIMD Extensions 4.2                */
    {(1 << 21), "X2APIC"},  /* ECX_X2APIC     | Extended xAPIC Support                       */
    {(1 << 22), "MOVBE"},   /* ECX_MOVBE      | MOVBE Instruction                            */
    {(1 << 23), "POPCNT"},  /* ECX_POPCNT     | POPCNT Instruction                           */
    {(1 << 24), "TSC"},     /* ECX_TSC        | Local APIC supports TSC Deadline             */
    {(1 << 25), "AESNI"},   /* ECX_AESNI      | AESNI Instruction                            */
    {(1 << 26), "XSAVE"},   /* ECX_XSAVE      | XSAVE/XSTOR States                           */
    {(1 << 27), "OSXSAVE"}, /* ECX_OSXSAVE    | OS Enabled Extended State Management         */
    {(1 << 28), "AVX"},     /* ECX_AVX        | AVX Instructions                             */
    {(1 << 29), "F16C"},    /* ECX_F16C       | 16-bit Floating Point Instructions           */
    {(1 << 30), "RDRAND"}   /* ECX_RDRAND     | RDRAND Instruction                           */
};


static inline void cpuid(uint32_t reg, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    ASM VOLATILE ("cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "0" (reg)
    );
}


// Function to check and print features or instructions
static void checkAndPrintInfo(const struct processor_info *table, uint32_t infoBits, const char* label) {
    printl(LINE, "%s", label);

    const int columnWidth = 30;

    // Iterate through each entry in the table
    for (int i = 0; i < 29; ++i) {
        const struct processor_info *info = &table[i];
        const char* name = info->name;

        // Print padding spaces
        for (int j = 0; j < 4; ++j) {
            printf(" ");
        }

        // Check if the feature or instruction is supported
        bool supported = (infoBits & info->bit) != 0;

        // Print feature or instruction name
        printl(supported ? "\033[92;40m*" : "\033[91;40m!", "%s", name);

        // Calculate padding spaces for alignment
        int padding = (columnWidth - (strlen(name) + 1)) - 4;

        // Print padding spaces
        for (int j = 0; j < padding; ++j) {
            printf(" ");
        }
    }

    printf("\n\n");
}


void processorGetStatus(void) {
    // Register storage
    uint32_t eax, ebx, ecx, edx;

    // Function 0x00 - Vendor-ID and Largest Standard Function

    uint32_t largestStandardFunc;
    char vendor[13];
    cpuid(0, &largestStandardFunc, (uint32_t *)(vendor + 0), (uint32_t *)(vendor + 8), (uint32_t *)(vendor + 4));
    vendor[12] = '\0';

    printl(LINE, "CPU Vendor: %s\n", vendor);

    // Function 0x01 - Feature Information

    if (largestStandardFunc >= 0x01) {
        cpuid(0x01, &eax, &ebx, &ecx, &edx);
        checkAndPrintInfo(features, edx, "Features:\n");
        checkAndPrintInfo(instructions, ecx, "Instructions:\n");
    }

    // Extended Function 0x00 - Largest Extended Function

    uint32_t largestExtendedFunc;
    cpuid(0x80000000, &largestExtendedFunc, &ebx, &ecx, &edx);

    // Extended Function 0x01 - Extended Feature Bits

    if (largestExtendedFunc >= 0x80000001) {
        cpuid(0x80000001, &eax, &ebx, &ecx, &edx);

        if (edx & EDX_64_BIT) {
            printl(INFO, "Its an 64-bit proccessor!\n\n");
        }
    }

    // Extended Function 0x02-0x04 - Processor Name / Brand String

    if (largestExtendedFunc >= 0x80000004) {
        char name[48];
        cpuid(0x80000002, (uint32_t *)(name +  0), (uint32_t *)(name +  4), (uint32_t *)(name +  8), (uint32_t *)(name + 12));
        cpuid(0x80000003, (uint32_t *)(name + 16), (uint32_t *)(name + 20), (uint32_t *)(name + 24), (uint32_t *)(name + 28));
        cpuid(0x80000004, (uint32_t *)(name + 32), (uint32_t *)(name + 36), (uint32_t *)(name + 40), (uint32_t *)(name + 44));

        // Processor name is right-justified with leading spaces
        const char *p = name;
        while (*p == ' ') {
            ++p;
        }

        printl(LINE, "CPU Name: %s\n", p);
    }
}


uint32_t processorGetTicks(void) {
    uint32_t ticks;
    ASM VOLATILE ("rdtsc" : "=a" (ticks));
    return ticks;
}
