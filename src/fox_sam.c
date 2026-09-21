// fox_sam.c — the fox's "critter" robot voice.
//
// ORIGINAL, clean-room formant speech synthesizer — NOT a copy of s-macke's
// reverse-engineered SAM (murky provenance). Converts English text to phonemes
// and renders them with a glottal source + 3 formant resonators, tuned HIGH and
// SOFT so it sounds like a cute little robot fox. Intelligible-ish; the on-
// screen caption carries the exact words.
//
// Drop-in ABI (a licensed sam.c exporting these will be used instead):
//   int  sam_render(text, speed, pitch, throat, mouth, uint8_t** out, int* len)
//   void sam_free(uint8_t* p)                 // 8-bit UNSIGNED mono @ 22050 Hz
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define SR      16000
#define MAX_SEC 7
#define MAX_SMP (SR * MAX_SEC)

typedef struct { uint16_t f1,f2,f3; uint8_t voiced,noise; float amp; uint8_t ms; } Phon;

enum { P_SIL,P_IY,P_IH,P_EH,P_AE,P_AA,P_AO,P_UH,P_UW,P_AH,P_ER,P_OW,P_AY,
       P_M,P_N,P_NG,P_L,P_R,P_W,P_Y,
       P_B,P_D,P_G,P_V,P_Z,P_DH,P_J,
       P_P,P_T,P_K,P_F,P_S,P_SH,P_TH,P_CH,P_H,P_NUM };

static const Phon PH[P_NUM] = {
    {0,0,0,0,0,0.0f,60},
    {300,2500,3300,1,0,0.9f,130},{430,2100,2650,1,0,0.9f,110},
    {580,1900,2550,1,0,0.95f,120},{720,1800,2500,1,0,1.0f,140},
    {800,1200,2500,1,0,1.0f,150},{620,950,2500,1,0,0.95f,140},
    {480,1100,2300,1,0,0.85f,110},{330,950,2250,1,0,0.85f,140},
    {700,1300,2450,1,0,0.95f,110},{520,1400,1700,1,0,0.9f,130},
    {560,900,2450,1,0,0.95f,150},{760,1600,2500,1,0,1.0f,170},
    {300,1100,2200,1,0,0.6f,90},{320,1600,2600,1,0,0.6f,85},
    {330,2000,2600,1,0,0.6f,90},{380,1300,2600,1,0,0.7f,80},
    {440,1300,1700,1,0,0.7f,80},{330,850,2200,1,0,0.7f,70},
    {300,2300,3000,1,0,0.7f,60},
    {300,1000,2200,1,0,0.5f,55},{320,1700,2600,1,0,0.5f,50},
    {330,1900,2500,1,0,0.5f,55},{400,1300,2400,1,1,0.4f,70},
    {330,1700,2600,1,1,0.45f,75},{350,1500,2500,1,1,0.4f,70},
    {330,1800,2500,1,1,0.5f,80},
    {300,900,2200,0,0,0.5f,50},{320,1800,2600,0,0,0.5f,45},
    {330,1900,2500,0,0,0.5f,55},{400,1300,2400,0,1,0.4f,80},
    {330,2000,3000,0,1,0.5f,90},{330,1900,2500,0,1,0.5f,95},
    {350,1600,2500,0,1,0.35f,75},{330,1900,2500,0,1,0.5f,90},
    {500,1500,2500,0,1,0.3f,55},
};

static int g2p(const char* t, uint8_t* ph, int maxn) {
    int n=0,i=0; int len=(int)strlen(t);
    #define EMIT(p) do{ if(n<maxn) ph[n++]=(uint8_t)(p); }while(0)
    #define M2(a,b) (tolower((unsigned char)t[i])==(a) && i+1<len && tolower((unsigned char)t[i+1])==(b))
    while (i<len && n<maxn) {
        char c=tolower((unsigned char)t[i]);
        if (isspace((unsigned char)c)||c=='.'||c==','||c=='!'||c=='?'||c=='~'){ EMIT(P_SIL); i++; continue; }
        if (M2('e','e')||M2('e','a')){ EMIT(P_IY); i+=2; continue; }
        if (M2('o','o')){ EMIT(P_UW); i+=2; continue; }
        if (M2('o','u')||M2('o','w')){ EMIT(P_OW); i+=2; continue; }
        if (M2('a','i')||M2('a','y')){ EMIT(P_AY); i+=2; continue; }
        if (M2('s','h')){ EMIT(P_SH); i+=2; continue; }
        if (M2('c','h')){ EMIT(P_CH); i+=2; continue; }
        if (M2('t','h')){ EMIT(P_TH); i+=2; continue; }
        if (M2('n','g')){ EMIT(P_NG); i+=2; continue; }
        if (M2('c','k')){ EMIT(P_K); i+=2; continue; }
        if (M2('p','h')){ EMIT(P_F); i+=2; continue; }
        if (M2('q','u')){ EMIT(P_K); EMIT(P_W); i+=2; continue; }
        if (M2('e','r')||M2('i','r')||M2('u','r')){ EMIT(P_ER); i+=2; continue; }
        if (M2('a','r')){ EMIT(P_AA); EMIT(P_R); i+=2; continue; }
        if (M2('o','r')){ EMIT(P_AO); EMIT(P_R); i+=2; continue; }
        switch (c) {
            case 'a': EMIT(P_AE); break; case 'e': EMIT(P_EH); break;
            case 'i': EMIT(P_IH); break; case 'o': EMIT(P_AA); break;
            case 'u': EMIT(P_AH); break; case 'y': EMIT(P_IY); break;
            case 'b': EMIT(P_B); break;  case 'c': EMIT(P_K); break;
            case 'd': EMIT(P_D); break;  case 'f': EMIT(P_F); break;
            case 'g': EMIT(P_G); break;  case 'h': EMIT(P_H); break;
            case 'j': EMIT(P_J); break;  case 'k': EMIT(P_K); break;
            case 'l': EMIT(P_L); break;  case 'm': EMIT(P_M); break;
            case 'n': EMIT(P_N); break;  case 'p': EMIT(P_P); break;
            case 'q': EMIT(P_K); break;  case 'r': EMIT(P_R); break;
            case 's': EMIT(P_S); break;  case 't': EMIT(P_T); break;
            case 'v': EMIT(P_V); break;  case 'w': EMIT(P_W); break;
            case 'x': EMIT(P_K); EMIT(P_S); break; case 'z': EMIT(P_Z); break;
            default: break;
        }
        i++;
    }
    if (n==0) EMIT(P_AH);
    return n;
}

typedef struct { float y1,y2,a1,a2,b0; } Reso;
static void reso_set(Reso* r, float f, float bw) {
    float rr=expf(-3.14159265f*bw/SR), th=2.0f*3.14159265f*f/SR;
    r->a1=2.0f*rr*cosf(th); r->a2=-rr*rr;
    float d=1.0f-2.0f*rr*cosf(2*th)+rr*rr; if(d<0)d=0;
    r->b0=(1.0f-rr)*sqrtf(d);
}
static inline float reso(Reso* r, float x) {
    float y=r->b0*x + r->a1*r->y1 + r->a2*r->y2;
    r->y2=r->y1; r->y1=y; return y;
}
static uint32_t rng=0x1234567u;
static inline float noise(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return ((int32_t)rng)/2147483648.0f; }

int sam_render(const char* text, uint8_t speed, uint8_t pitch,
               uint8_t throat, uint8_t mouth, uint8_t** out, int* out_len) {
    (void)throat; (void)mouth;
    if (!text||!out||!out_len) return 0;
    uint8_t ph[256]; int np=g2p(text, ph, 256);
    float f0 = 180.0f + (pitch/255.0f)*120.0f;
    float dur_scale = 0.7f + (speed/255.0f)*0.9f;
    uint8_t* buf=(uint8_t*)malloc(MAX_SMP); if(!buf) return 0;
    int pos=0;
    Reso r1,r2,r3; memset(&r1,0,sizeof r1); memset(&r2,0,sizeof r2); memset(&r3,0,sizeof r3);
    float cf1=500,cf2=1500,cf3=2500, phase=0.0f;
    for (int p=0; p<np && pos<MAX_SMP-1; ++p) {
        const Phon* q=&PH[ph[p]];
        int samps=(int)(q->ms*dur_scale*SR/1000.0f); if(samps<1) samps=1;
        float tf1=q->f1?q->f1:cf1, tf2=q->f2?q->f2:cf2, tf3=q->f3?q->f3:cf3;
        int fade=SR*6/1000;
        for (int s=0; s<samps && pos<MAX_SMP-1; ++s) {
            float g=(float)s/samps;
            reso_set(&r1, cf1+(tf1-cf1)*g, 90);
            reso_set(&r2, cf2+(tf2-cf2)*g, 110);
            reso_set(&r3, cf3+(tf3-cf3)*g, 170);
            float src;
            if (q->amp<=0.001f) src=0;
            else if (q->noise && !q->voiced) src=noise()*0.9f;
            else {
                phase += f0/SR; float imp=0;
                if (phase>=1.0f){ phase-=1.0f; imp=1.0f; }
                src = imp - (phase<0.05f?0.5f:0.0f);
                if (q->noise) src += noise()*0.25f;
            }
            float v = (reso(&r1,src)*1.0f + reso(&r2,src)*0.7f + reso(&r3,src)*0.3f) * 9.0f;
            v *= q->amp;
            if (s<fade) v*=(float)s/fade;
            if (s>samps-fade) v*=(float)(samps-s)/fade;
            v = tanhf(v*1.4f);
            int u = 128 + (int)(v*110); if(u<0)u=0; if(u>255)u=255;
            buf[pos++]=(uint8_t)u;
        }
        cf1=tf1; cf2=tf2; cf3=tf3;
    }
    if (pos<1){ buf[0]=128; pos=1; }
    *out=buf; *out_len=pos;
    return 1;
}
void sam_free(uint8_t* p){ free(p); }
