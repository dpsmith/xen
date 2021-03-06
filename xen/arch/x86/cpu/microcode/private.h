#ifndef ASM_X86_MICROCODE_PRIVATE_H
#define ASM_X86_MICROCODE_PRIVATE_H

#include <xen/types.h>

#include <asm/microcode.h>

enum microcode_match_result {
    OLD_UCODE, /* signature matched, but revision id is older or equal */
    NEW_UCODE, /* signature matched, but revision id is newer */
    MIS_UCODE, /* signature mismatched */
};

struct microcode_patch {
    union {
        struct microcode_intel *mc_intel;
        struct microcode_amd *mc_amd;
        void *mc;
    };
};

struct microcode_ops {
    /*
     * Parse a microcode container.  Format is vendor-specific.
     *
     * Search within the container for the patch, suitable for the current
     * CPU, which has the highest revision.  (Note: May be a patch which is
     * older that what is running in the CPU.  This is a feature, to better
     * cope with corner cases from buggy firmware.)
     *
     * If one is found, allocate and return a struct microcode_patch
     * encapsulating the appropriate microcode patch.  Does not alias the
     * original buffer.
     *
     * If one is not found, (nothing matches the current CPU), return NULL.
     * Also may return ERR_PTR(-err), e.g. bad container, out of memory.
     */
    struct microcode_patch *(*cpu_request_microcode)(const void *buf,
                                                     size_t size);

    /* Obtain microcode-relevant details for the current CPU. */
    int (*collect_cpu_info)(struct cpu_signature *csig);

    /*
     * Attempt to load the provided patch into the CPU.  Returns an error if
     * anything didn't go as expected.
     */
    int (*apply_microcode)(const struct microcode_patch *patch);

    /*
     * Optional.  If provided and applicable to the specific update attempt,
     * is run once by the initiating CPU.  Returning an error will abort the
     * load attempt.
     */
    int (*start_update)(void);

    /*
     * Optional.  If provided, called on every CPU which completes a microcode
     * load.  May be called in the case of some errors, and not others.  May
     * be called even if start_update() wasn't.
     */
    void (*end_update_percpu)(void);

    /* Free a patch previously allocated by cpu_request_microcode(). */
    void (*free_patch)(void *mc);

    /*
     * Is the microcode patch applicable for the current CPU, and newer than
     * the currently running patch?
     */
    bool (*match_cpu)(const struct microcode_patch *patch);

    /*
     * Given two patches, are they both applicable to the current CPU, and is
     * new a higher revision than old?
     */
    enum microcode_match_result (*compare_patch)(
        const struct microcode_patch *new, const struct microcode_patch *old);
};

extern const struct microcode_ops amd_ucode_ops, intel_ucode_ops;

#endif /* ASM_X86_MICROCODE_PRIVATE_H */
