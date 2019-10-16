/******************************************************************************
 * txt.h
 *
 * Control domain TXT/TPM services.
 *
 * Copyright (c) 2017 Assured Information Security, Inc
 *
 * Authors:
 * Ross Philipson <philipsonr@ainfosec.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __XEN_PUBLIC_TXT_H__
#define __XEN_PUBLIC_TXT_H__

#include "xen.h"

/* version of ABI */
#define TXT_SPEC_VERSION          1

/*
 * Return TXT TPM event log
 *
 * @arg == pointer to xen_txt_evtlog_t input/output structure.
 */
#define TXTOP_evtlog    0

struct xen_txt_evtlog {
    /* IN/OUT */
    uint64_t  size;
    /* OUT */
    uint8_t format;
    uint8_t _pad[7];
    /* OUT */
    XEN_GUEST_HANDLE(void) buffer;
};
typedef struct xen_txt_evtlog xen_txt_evtlog_t;
DEFINE_XEN_GUEST_HANDLE(xen_txt_evtlog_t);

#endif /* __XEN_PUBLIC_TXT_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
