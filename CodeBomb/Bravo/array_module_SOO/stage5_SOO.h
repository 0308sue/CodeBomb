#ifndef STAGE5_SOO_H
#define STAGE5_SOO_H
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define STRING_CAP 12
#define STRING_FRAMES 32
/* 한글: initialized는 빈 메모리와 실제 널 문자를 구분한다.
 * English: An initialization mask distinguishes unwritten slots from a real null byte. */
typedef struct {
    char first[STRING_CAP],second[STRING_CAP];
    unsigned initialized;
    int firstAt,secondAt,count,result;
    bool done,failed;
    const char *note;
} StringFrame;
typedef struct {
    int level,step,n,answer,capacity;
    bool playing;
    float timer;
    char input[STRING_CAP];
    StringFrame frames[STRING_FRAMES];
} StringLesson;
typedef struct { const char *title,*goal,*code,*question,*choices[3],*reason; int answer; } StringTask;
static const StringTask STRING_TASKS[]={
 {"Password Length","Count HELLO without counting its null terminator.",
  "int strlen_ptr(char *str) {\n    int length = 0;\n    while (*str != '\\0') {\n        length++; str++;\n    }\n    return length;\n}",
  "What is the length of HELLO?",{"5","6","4"},"The final null byte marks the end. It is not a letter.",0},
 {"Vowel Scanner","Count upper and lower case vowels in OpenAI.",
  "int count_vowels(char *str) {\n    int count = 0;\n    for (; *str; str++) {\n        char c = *str;\n        if (c=='a'||c=='e'||c=='i'||c=='o'||c=='u'\n          ||c=='A'||c=='E'||c=='I'||c=='O'||c=='U')\n            count++;\n    }\n    return count;\n}",
  "How many vowels does OpenAI contain?",{"3","4","5"},"O, e, A and I are vowels. Both cases count.",1},
 {"Copy the Password","Copy CODE and its null terminator into destination.",
  "void copy_string(char *dest, char *src) {\n    while (*src) {\n        *dest++ = *src++;\n    }\n    *dest = '\\0';\n}\n// The destination must have enough space.",
  "How many bytes must be copied?",{"4","6","5"},"CODE needs four letters plus one null byte.",2},
 {"Combine the Codes","Append SAFE to BOMB, including a new terminator.",
  "void concat_strings(char *dest, char *src) {\n    while (*dest) dest++;\n    while (*src) *dest++ = *src++;\n    *dest = '\\0';\n}\n// At least 9 destination bytes required.\n// The game checks capacity before writing.",
  "How much space does BOMBSAFE need?",{"8","9","10"},"Eight letters need nine bytes, including the final null byte.",1},
 {"Password Verification","Compare stored BOMB with your input, in that order.",
  "int compare_strings(char *a, char *b) {\n    while (*a && *a == *b) {\n        a++; b++;\n    }\n    return (unsigned char)*a\n         - (unsigned char)*b;\n}",
  "",{"","",""},"The first different bytes decide the sign. Zero means equal.",0}
};

/* 한글: 비교는 unsigned char 값으로 한다. 접두어도 널 문자를 비교해 구분한다.
 * English: Compare unsigned byte values; comparing terminators handles prefixes too. */
static int StringCompare(const char *a,const char *b)
{
    while(*a && *a==*b) {a++;b++;}
    return (unsigned char)*a-(unsigned char)*b;
}
static bool StringVowel(char c)
{
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'||c=='A'||c=='E'||c=='I'||c=='O'||c=='U';
}
/* 한글: 예제에서 실제 포인터 연산을 수행하고 값만 복사해 보관한다.
 * English: Perform pointer operations in the built-in example and save only values. */
static void StringBuild(StringLesson *v)
{
    v->step=0;v->n=0;v->playing=false;v->timer=0;
    StringFrame f={.firstAt=-1,.secondAt=-1};
    const char *start[]={"HELLO","OpenAI","CODE","SAFE","BOMB"};
    strcpy(f.first,start[v->level]);
    if(v->level==3) {strcpy(f.second,"BOMB");f.initialized=31u;}
    if(v->level==4) {strcpy(f.second,v->input);f.initialized=(1u<<(strlen(v->input)+1))-1;}
    #define SAVE(msg) do { f.note=(msg); v->frames[v->n++]=f; } while(0)
    SAVE(v->level==4?"Enter a password and press Verify.":"Choose an answer to reveal the sample.");
    char *src=f.first,*dest=f.second;
    if(v->level==0 || v->level==1) {
        while(*src) {
            f.firstAt=(int)(src-f.first);
            if(v->level==0 || StringVowel(*src)) f.count++;
            SAVE(v->level==0?"Read a letter and add one to the length.":StringVowel(*src)?"Vowel found. Increase the count.":"Not a vowel. Keep the count.");
            src++;
        }
        f.firstAt=(int)(src-f.first);f.result=f.count;
        SAVE("Null terminator found. Stop without counting it.");
    } else if(v->level==2 || v->level==3) {
        size_t offset=v->level==3?strlen(f.second):0;
        size_t required=offset+strlen(src)+1;
        if(required>(size_t)v->capacity) {
            f.done=true;f.failed=true;
            SAVE("Not enough destination space. No bytes were written.");
            return;
        }
        if(v->level==3) {
            while(*dest) {
                f.secondAt=(int)(dest-f.second);
                SAVE("Move destination pointer toward its null terminator.");dest++;
            }
        }
        do {
            f.firstAt=(int)(src-f.first);f.secondAt=(int)(dest-f.second);
            *dest=*src;f.initialized|=1u<<f.secondAt;
            SAVE(*src?"Read source and write this character to destination.":"Copy the null byte to terminate the destination.");
            if(!*src) break;
            src++;dest++;
        } while(true);
    } else {
        while(true) {
            f.firstAt=(int)(src-f.first);f.secondAt=(int)(dest-f.second);
            f.result=(unsigned char)*src-(unsigned char)*dest;
            SAVE(f.result?"Different bytes found. Return their difference.":!*src?"Both terminators match. Return zero.":"These letters match. Move both pointers.");
            if(f.result || !*src) break;
            src++;dest++;
        }
    }
    if(v->level==4) f.result=StringCompare(f.first,f.second);
    f.done=true;
    SAVE(v->level==4?(f.result==0?"Passwords match. Device unlocked.":"Passwords differ. Check the first different bytes."):"Sample complete. Review any step with Back.");
    #undef SAVE
}
static void StringLoad(StringLesson *v,int level)
{
    *v=(StringLesson){.level=level,.answer=-1,.capacity=STRING_CAP};
    strcpy(v->input,"BOMB");StringBuild(v);
}
#endif
