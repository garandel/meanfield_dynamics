/*
 * Copyright (c) 2017-2019 The University of Manchester
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//! \file
//! \brief complementary error function in fix number arithmetics

#include <debug.h>
#include <math.h>
#include "../../../src/common/maths-util.h"
//#include <stdfix-exp.h>
//#include <polynomial.h>
#include <stdfix-full-iso.h>

/* origin: FreeBSD /usr/src/lib/msun/src/s_erf.c */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* double erf(double x)
 * double erfc(double x)
 *                           x
 *                    2      |\
 *     erf(x)  =  ---------  | exp(-t*t)dt
 *                 sqrt(pi) \|
 *                           0
 *
 *     erfc(x) =  1-erf(x)
 *  Note that
 *              erf(-x) = -erf(x)
 *              erfc(-x) = 2 - erfc(x)
 *
 * Method:
 *      1. For |x| in [0, 0.84375]
 *          erf(x)  = x + x*R(x^2)
 *          erfc(x) = 1 - erf(x)           if x in [-.84375,0.25]
 *                  = 0.5 + ((0.5-x)-x*R)  if x in [0.25,0.84375]
 *         where R = P/Q where P is an odd poly of degree 8 and
 *         Q is an odd poly of degree 10.
 *                                               -57.90
 *                      | R - (erf(x)-x)/x | <= 2
 *
 *
 *         Remark. The formula is derived by noting
 *          erf(x) = (2/sqrt(pi))*(x - x^3/3 + x^5/10 - x^7/42 + ....)
 *         and that
 *          2/sqrt(pi) = 1.128379167095512573896158903121545171688
 *         is close to one. The interval is chosen because the fix
 *         point of erf(x) is near 0.6174 (i.e., erf(x)=x when x is
 *         near 0.6174), and by some experiment, 0.84375 is chosen to
 *         guarantee the error is less than one ulp for erf.
 *
 *      2. For |x| in [0.84375,1.25], let s = |x| - 1, and
 *         c = 0.84506291151 rounded to single (24 bits)
 *              erf(x)  = sign(x) * (c  + P1(s)/Q1(s))
 *              erfc(x) = (1-c)  - P1(s)/Q1(s) if x > 0
 *                        1+(c+P1(s)/Q1(s))    if x < 0
 *              |P1/Q1 - (erf(|x|)-c)| <= 2**-59.06
 *         Remark: here we use the taylor series expansion at x=1.
 *              erf(1+s) = erf(1) + s*Poly(s)
 *                       = 0.845.. + P1(s)/Q1(s)
 *         That is, we use rational approximation to approximate
 *                      erf(1+s) - (c = (single)0.84506291151)
 *         Note that |P1/Q1|< 0.078 for x in [0.84375,1.25]
 *         where
 *              P1(s) = degree 6 poly in s
 *              Q1(s) = degree 6 poly in s
 *
 *      3. For x in [1.25,1/0.35(~2.857143)],
 *              erfc(x) = (1/x)*exp(-x*x-0.5625+R1/S1)
 *              erf(x)  = 1 - erfc(x)
 *         where
 *              R1(z) = degree 7 poly in z, (z=1/x^2)
 *              S1(z) = degree 8 poly in z
 *
 *      4. For x in [1/0.35,28]
 *              erfc(x) = (1/x)*exp(-x*x-0.5625+R2/S2) if x > 0
 *                      = 2.0 - (1/x)*exp(-x*x-0.5625+R2/S2) if -6<x<0
 *                      = 2.0 - tiny            (if x <= -6)
 *              erf(x)  = sign(x)*(1.0 - erfc(x)) if x < 6, else
 *              erf(x)  = sign(x)*(1.0 - tiny)
 *         where
 *              R2(z) = degree 6 poly in z, (z=1/x^2)
 *              S2(z) = degree 7 poly in z
 *
 *      Note1:
 *         To compute exp(-x*x-0.5625+R/S), let s be a single
 *         precision number and s := x; then
 *              -x*x = -s*s + (s-x)*(s+x)
 *              exp(-x*x-0.5626+R/S) =
 *                      exp(-s*s-0.5625)*exp((s-x)*(s+x)+R/S);
 *      Note2:
 *         Here 4 and 5 make use of the asymptotic series
 *                        exp(-x*x)
 *              erfc(x) ~ ---------- * ( 1 + Poly(1/x^2) )
 *                        x*sqrt(pi)
 *         We use rational approximation to approximate
 *              g(s)=f(1/x^2) = log(erfc(x)*x) - x*x + 0.5625
 *         Here is the error bound for R1/S1 and R2/S2
 *              |R1/S1 - f(x)|  < 2**(-62.57)
 *              |R2/S2 - f(x)|  < 2**(-61.52)
 *
 *      5. For inf > x >= 28
 *              erf(x)  = sign(x) *(1 - tiny)  (raise inexact)
 *              erfc(x) = tiny*tiny (raise underflow) if x > 0
 *                      = 2 - tiny if x<0
 *
 *      7. Special case:
 *              erf(0)  = 0, erf(inf)  = 1, erf(-inf) = -1,
 *              erfc(0) = 1, erfc(inf) = 0, erfc(-inf) = 2,
 *              erfc/erf(NaN) is NaN
 */

//#include "libm.h"
//static const REAL_N
/* Set the less significant 32 bits of a double from an int.  */
#define SET_LOW_WORD(d,lo)                        \
do {                                              \
  union {REAL f; uint64_t i;} __u;              \
  __u.f = (d);                                    \
  __u.i &= 0xffffffff00000000ull;                 \
  __u.i |= (uint32_t)(lo);                        \
  (d) = __u.f;                                    \
} while (0)

/* Get the more significant 32 bit int from a double.  */
#define GET_HIGH_WORD(hi,d)                       \
do {                                              \
  union {REAL f; uint64_t i;} __u;              \
  __u.f = (d);                                    \
  (hi) = __u.i >> 32;                             \
} while (0)



#define erx 8.45062911510467529297e-01 /* 0x3FEB0AC1, 0x60000000 */
/*
 * Coefficients for approximation to  erf on [0,0.84375]
 */
#define efx8 1.027033e+00 /* 0x3FF06EBA, 0x8214DB69 */
#define pp0 1.283792e-01 /* 0x3FC06EBA, 0x8214DB68 */
#define pp1 -3.250421e-01 /* 0xBFD4CD7D, 0x691CB913 */
#define pp2 -2.848175e-02 /* 0xBF9D2A51, 0xDBD7194F */
#define pp3 -5.770270e-03 /* 0xBF77A291, 0x236668E4 */
#define pp4 -2.376302e-05 /* 0xBEF8EAD6, 0x120016AC */
#define qq1 3.9791722e-01 /* 0x3FD97779, 0xCDDADC09 */
#define qq2 6.5022250e-02 /* 0x3FB0A54C, 0x5536CEBA */
#define qq3 5.0813063e-03 /* 0x3F74D022, 0xC4D36B0F */
#define qq4 1.3249474e-04 /* 0x3F215DC9, 0x221C1A10 */
#define qq5 -3.960228e-06 /* 0xBED09C43, 0x42A26120 */
/*
 * Coefficients for approximation to  erf  in [0.84375,1.25]
 */
#define pa0 -2.362190e-03 /* 0xBF6359B8, 0xBEF77538 */
#define pa1 4.148561e-01 /* 0x3FDA8D00, 0xAD92B34D */
#define pa2 -3.722079e-01 /* 0xBFD7D240, 0xFBB8C3F1 */
#define pa3 3.183466e-01 /* 0x3FD45FCA, 0x805120E4 */
#define pa4 -1.108947e-01 /* 0xBFBC6398, 0x3D3E28EC */
#define pa5 3.547830e-02 /* 0x3FA22A36, 0x599795EB */
#define pa6 -2.166376e-03 /* 0xBF61BF38, 0x0A96073F */
#define qa1 1.064209e-01 /* 0x3FBB3E66, 0x18EEE323 */
#define qa2 5.403979e-01 /* 0x3FE14AF0, 0x92EB6F33 */
#define qa3 7.182865e-02 /* 0x3FB2635C, 0xD99FE9A7 */
#define qa4 1.261712e-01 /* 0x3FC02660, 0xE763351F */
#define qa5 1.363708e-02 /* 0x3F8BEDC2, 0x6B51DD1C */
#define qa6 1.198450e-02 /* 0x3F888B54, 0x5735151D */
/*
 * Coefficients for approximation to  erfc in [1.25,1/0.35]
 */
#define ra0 -9.86494403484714822705e-03 /* 0xBF843412, 0x600D6435 */
#define ra1 -6.93858572707181764372e-01 /* 0xBFE63416, 0xE4BA7360 */
#define ra2 -1.05586262253232909814e+01 /* 0xC0251E04, 0x41B0E726 */
#define ra3 -6.23753324503260060396e+01 /* 0xC04F300A, 0xE4CBA38D */
#define ra4 -1.62396669462573470355e+02 /* 0xC0644CB1, 0x84282266 */
#define ra5 -1.84605092906711035994e+02 /* 0xC067135C, 0xEBCCABB2 */
#define ra6 -8.12874355063065934246e+01 /* 0xC0545265, 0x57E4D2F2 */
#define ra7 -9.81432934416914548592e+00 /* 0xC023A0EF, 0xC69AC25C */
#define sa1 1.96512716674392571292e+01 /* 0x4033A6B9, 0xBD707687 */
#define sa2 1.37657754143519042600e+02 /* 0x4061350C, 0x526AE721 */
#define sa3 4.34565877475229228821e+02 /* 0x407B290D, 0xD58A1A71 */
#define sa4 6.45387271733267880336e+02 /* 0x40842B19, 0x21EC2868 */
#define sa5 4.29008140027567833386e+02 /* 0x407AD021, 0x57700314 */
#define sa6 1.08635005541779435134e+02 /* 0x405B28A3, 0xEE48AE2C */
#define sa7 6.57024977031928170135e+00 /* 0x401A47EF, 0x8E484A93 */
#define sa8 -6.04244152148580987438e-02 /* 0xBFAEEFF2, 0xEE749A62 */
/*
 * Coefficients for approximation to  erfc in [1/.35,28]
 */
#define rb0 -9.86494292470009928597e-03 /* 0xBF843412, 0x39E86F4A */
#define rb1 -7.99283237680523006574e-01 /* 0xBFE993BA, 0x70C285DE */
#define rb2 -1.77579549177547519889e+01 /* 0xC031C209, 0x555F995A */
#define rb3 -1.60636384855821916062e+02 /* 0xC064145D, 0x43C5ED98 */
#define rb4 -6.37566443368389627722e+02 /* 0xC083EC88, 0x1375F228 */
#define rb5 -1.02509513161107724954e+03 /* 0xC0900461, 0x6A2E5992 */
#define rb6 -4.83519191608651397019e+02 /* 0xC07E384E, 0x9BDC383F */
#define sb1 3.03380607434824582924e+01 /* 0x403E568B, 0x261D5190 */
#define sb2 3.25792512996573918826e+02 /* 0x40745CAE, 0x221B9F0A */
#define sb3 1.53672958608443695994e+03 /* 0x409802EB, 0x189D5118 */
#define sb4 3.19985821950859553908e+03 /* 0x40A8FFB7, 0x688C246A */
#define sb5 2.55305040643316442583e+03 /* 0x40A3F219, 0xCEDF3BE6 */
#define sb6 4.74528541206955367215e+02 /* 0x407DA874, 0xE79FE763 */
#define sb7 -2.24409524465858183362e+01 /* 0xC03670E2, 0x42712D62 */
/*
static union {
    s1615 as_s1615;
    input_t as_real;
} number;

static inline REAL exp_changer(REAL x)
{
    
    number.as_real = x;//*exc_syn_values;//
    s1615 x_s1615 = number.as_s1615; 
    
    number.as_s1615 = expk(x_s1615);
    input_t result = number.as_real;
       
    
    //REAL result = erfc(x); //region `ITCM' overflowed by 3448 bytes
        
    //log_info("IT'S THE WRONG ONE JUST FOR TEST => NEED ERFC NOT ONLY EXPK ");
    //return __horner_int_b(poly,x,2);
    //return expk(x_s1615);
    return result;
    //return expf(x);
    //return x+1.*20;
        
}
*/
/*
static inline REAL abs_changer(REAL x)
{
    
    number.as_real = x;//*exc_syn_values;//
    s1615 x_s1615 = number.as_s1615; 
    
    number.as_s1615 = absk(x_s1615);
    input_t result = number.as_real;

    return result;
    
        
} 
*/

 //fait gagner vite fait 20 bytes
static double fonc_abs(double x){
    if(x<0){
        x=-1*x;
    }
    
    return x;
};

static double division_light(double num, double den){
    uint32_t count;
    uint8_t count_limit;
    uint32_t count_decimal;
    while (1) {
        uint32_t result = num - den;
        count++;
        if (result == 0) {
                count++;
                break;
        }
        if (result < den){
            result *= 10;
            count_limit++;
            if (count_limit == 6){
                break;
            }
        }
        if (result >= den){
            result -= den;
            count_decimal++;
        }

    }
}

static double erfc1(double x)
{
	input_t s,P,Q;
    
    /*input_t p_inter_1, p_inter_2, p_inter_3;
    input_t p_inter_4, p_inter_5;
    
    input_t q_inter_1, q_inter_2, q_inter_3;
    input_t q_inter_4, q_inter_5;
    */
    
	s = fonc_abs(x) - 1;
    /*
    p_inter_1 = pa5+s*pa6;
    p_inter_2 = pa4+s*(p_inter_1);
    p_inter_3 = pa3+s*(p_inter_2);
    p_inter_4 = pa2+s*(p_inter_3);
    p_inter_5 = pa1+s*(p_inter_4);
    P = pa0+s*(p_inter_5);
    */
    
	P = pa0+s*(pa1+s*(pa2+s*(pa3+s*(pa4+s*(pa5+s*pa6)))));
    /*
    input_t P1 = pa0 + s*pa1;
    input_t P2 = pa2 + s*pa3;
    input_t P3 = pa4 + s*pa5;
    input_t s2 = s*s;
    input_t s4 = s2*s2;
    input_t s6 = s2*s4;
    
    P = P1 + s2*P2 + s4*P3 + s6*pa6;
    */
    /*
    q_inter_1 = qa5+s*qa6;
    q_inter_2 = qa4+s*(q_inter_1);
    q_inter_3 = qa3+s*(q_inter_2);
    q_inter_4 = qa2+s*(q_inter_3);
    q_inter_5 = qa1+s*(q_inter_4);
    Q = 1+s*q_inter_5;
    */
    
	Q = 1+s*(qa1+s*(qa2+s*(qa3+s*(qa4+s*(qa5+s*qa6)))));
    /*
    input_t Q1 = 1 + s*qa1;
    input_t Q2 = qa2 + s*qa3;
    input_t Q3 = qa4 + s*qa5;
    
    Q = Q1 + s2*Q2 + s4*Q3 + s6*qa6;
    */
	return 1 - erx - P/Q;
}

static double erfc2(uint32_t ix, double x)
{
	input_t s,R,S;
	REAL z;

	if (ix < 0x3ff40000)  /* |x| < 1.25 */
		return erfc1(x);

	x = fonc_abs(x);
	s = 1/(x*x);
	if (ix < 0x4006db6d) {  /* |x| < 1/.35 ~ 2.85714 */
		R = ra0+s*(ra1+s*(ra2+s*(ra3+s*(ra4+s*(
		     ra5+s*(ra6+s*ra7))))));
		S = 1.0+s*(sa1+s*(sa2+s*(sa3+s*(sa4+s*(
		     sa5+s*(sa6+s*(sa7+s*sa8)))))));
	} else {                /* |x| > 1/.35 */
		R = rb0+s*(rb1+s*(rb2+s*(rb3+s*(rb4+s*(
		     rb5+s*rb6)))));
		S = 1.0+s*(sb1+s*(sb2+s*(sb3+s*(sb4+s*(
		     sb5+s*(sb6+s*sb7))))));
	}
	z = x;
	SET_LOW_WORD(z,0);
	return EXP(-z*z-0.5625)*EXP((z-x)*(z+x)+R/S)/x; //EXP
}

double erf(double x)
{
	REAL r,s,z,y;
	uint32_t ix;
	int sign;

	GET_HIGH_WORD(ix, x);
	sign = ix>>31;
	ix &= 0x7fffffff;
	if (ix >= 0x7ff00000) {
		/* erf(nan)=nan, erf(+-inf)=+-1 */
		return 1-2*sign + 1/x;
	}
	if (ix < 0x3feb0000) {  /* |x| < 0.84375 */
		if (ix < 0x3e300000) {  /* |x| < 2**-28 */
			/* avoid underflow */
			return 0.125*(8*x + efx8*x);
		}
		z = x*x;
		r = pp0+z*(pp1+z*(pp2+z*(pp3+z*pp4)));
		s = 1.0+z*(qq1+z*(qq2+z*(qq3+z*(qq4+z*qq5))));
		y = r/s;
		return x + x*y;
	}
	if (ix < 0x40180000)  /* 0.84375 <= |x| < 6 */
		y = 1 - erfc2(ix,x);
	else
		y = 1 - 0x1p-1022;
	return sign ? -y : y;
}

double erfc(double x)
{
	REAL r,s,z,y;
	uint32_t ix;
	int sign;

	GET_HIGH_WORD(ix, x);
	sign = ix>>31;
	ix &= 0x7fffffff;
	if (ix >= 0x7ff00000) {
		/* erfc(nan)=nan, erfc(+-inf)=0,2 */
		return 2*sign + 1/x;
	}
	if (ix < 0x3feb0000) {  /* |x| < 0.84375 */
		if (ix < 0x3c700000)  /* |x| < 2**-56 */
			return 1.0 - x;
		z = x*x;
		r = pp0+z*(pp1+z*(pp2+z*(pp3+z*pp4)));
		s = 1.0+z*(qq1+z*(qq2+z*(qq3+z*(qq4+z*qq5))));
		y = r/s;
		if (sign || ix < 0x3fd00000) {  /* x < 1/4 */
			return 1.0 - (x+x*y);
		}
		return 0.5 - (x - 0.5 + x*y);
	}
	if (ix < 0x403c0000) {  /* 0.84375 <= |x| < 28 */
		return sign ? 2 - erfc2(ix,x) : erfc2(ix,x);
	}


	return sign ? 2 - 0x1p-1022 : 0x1p-1022*0x1p-1022;
}



