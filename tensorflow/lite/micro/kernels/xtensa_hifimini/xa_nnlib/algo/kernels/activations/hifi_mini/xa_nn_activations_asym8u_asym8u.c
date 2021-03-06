/*******************************************************************************
* Copyright (c) 2018-2020 Cadence Design Systems, Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to use this Software with Cadence processor cores only and
* not with any other processors and platforms, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************************/
#include "xa_nnlib_common.h"

#define ALIGNMENT   8   /* 8 bytes alignment */

#define ALIGN_PTR(x, bytes)     ((((unsigned)(x))+(bytes-1))&(~(bytes-1)))

#define LIMIT(out, inp, min, max){\
        out = min;\
        out = AE_MAXP24S(inp, min);\
        out = AE_MINP24S(out, max);\
}

#define STORE_8X2_FROM_24X2(out_ptr, val){\
    int o1, o2;\
    o1 = AE_MOVAP24S_H(val);\
    o2 = AE_MOVAP24S_L(val);\
    *out_ptr++ = (UWORD8)o1;\
    *out_ptr++ = (UWORD8)o2;\
}

/*
 * inp: p_vec: 4 byte aligned input pointer
 * out: p_out: no alignment needed for output pointer*/
WORD32 xa_nn_vec_activation_min_max_asym8u_asym8u(UWORD8 * __restrict__ p_out,
                                      const  UWORD8 * __restrict__ p_vec,
                                      int    activation_min,
                                      int    activation_max,
                                      WORD32 vec_length)
{
    int i;
    ae_p24x2s x, y, min, max;

    /* NULL pointer checks */
    XA_NNLIB_ARG_CHK_PTR(p_out, -1);
    XA_NNLIB_ARG_CHK_PTR(p_vec, -1);

    /* Basic Parameter checks */
    XA_NNLIB_ARG_CHK_COND((vec_length <= 0), -1);

    /* Basic Parameter checks */
    XA_NNLIB_ARG_CHK_COND((activation_max < activation_min), -1);

    UWORD8 *p_o = p_out;
    UWORD8 *p_v = (UWORD8 *)p_vec;

    min  = AE_SRLIP24(AE_CVTP24A16(activation_min), 8);
    max  = AE_SRLIP24(AE_CVTP24A16(activation_max), 8);

    int pre_loop_count=0;
    // pre loop, active when input ptr is not 4 byte aligned
    pre_loop_count = (int)((unsigned)ALIGN_PTR(p_v, 4) - (unsigned)p_v);
    pre_loop_count = (pre_loop_count < vec_length) ? pre_loop_count : vec_length;

    vec_length = vec_length - pre_loop_count;
    vec_length = (vec_length < 0) ? 0 : vec_length;

    for(i=0; i<pre_loop_count; i++)
    {
        int i1;
        i1 = ((UWORD8)*p_v++);
        x  = AE_MOVPA24(i1);
        LIMIT(y, x, min, max)
        i1 = AE_MOVAP24S_H(y);
        *p_o++ = (UWORD8)i1;
    }

    if((activation_max >= (int)255) && (activation_min <= (int)0))
    {
        p_v = p_v - 2;
        for(i=0; i<(vec_length >> 1); i++)
        {
            AE_LP8X2F_IU(x, (WORD8 *)p_v, 2*sizeof(WORD8));
            y = AE_SRLIP24(x, 16);

            STORE_8X2_FROM_24X2(p_o, y)
        }
        if(vec_length & 1)
        {
            p_v = p_v + 2;
            int i1;
            i1 = (UWORD8) p_v[0];
            *p_o++ = (UWORD8)i1;
        }
    }
    else if((activation_max < (int)255) && (activation_min <= (int)0))
    {
        p_v = p_v - 2;
        for(i=0; i<(vec_length >> 1); i++)
        {
            AE_LP8X2F_IU(x, (WORD8 *)p_v, 2*sizeof(WORD8));
            y = AE_SRLIP24(x, 16);

            y = AE_MINP24S(y, max);

            STORE_8X2_FROM_24X2(p_o, y)
        }
        if(vec_length & 1)
        {
            p_v = p_v + 2;
            int i1;
            i1 = (UWORD8) p_v[0];
            y  = AE_MOVPA24(i1);

            y = AE_MINP24S(y, max);

            i1 = AE_MOVAP24S_H(y);
            *p_o++ = (UWORD8)i1;
        }
    }
    else if((activation_max >= (int)255) && (activation_min > (int)0))
    {
        p_v = p_v - 2;
        for(i=0; i<(vec_length >> 1); i++)
        {
            AE_LP8X2F_IU(x, (WORD8 *)p_v, 2*sizeof(WORD8));
            y = AE_SRLIP24(x, 16);

            y = AE_MAXP24S(y, min);

            STORE_8X2_FROM_24X2(p_o, y)
        }
        if(vec_length & 1)
        {
            p_v = p_v + 2;
            int i1;
            i1 = (UWORD8) p_v[0];
            y  = AE_MOVPA24(i1);

            y = AE_MAXP24S(y, min);

            i1 = AE_MOVAP24S_H(y);
            *p_o++ = (UWORD8)i1;
        }
    }
    else
    {
        p_v = p_v - 2;
        for(i=0; i<(vec_length >> 1); i++)
        {
            AE_LP8X2F_IU(x, (WORD8 *)p_v, 2*sizeof(WORD8));
            x = AE_SRLIP24(x, 16);
            LIMIT(y, x, min, max)
            STORE_8X2_FROM_24X2(p_o, y)
        }
        if(vec_length & 1)
        {
            p_v = p_v + 2;
            int i1;
            i1 = (UWORD8) p_v[0];
            x  = AE_MOVPA24(i1);
            LIMIT(y, x, min, max)
            i1 = AE_MOVAP24S_H(y);
            *p_o++ = (UWORD8)i1;
        }
    }
    return 0;
}
