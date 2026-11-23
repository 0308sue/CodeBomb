#include "stage8_SOO.h"
#include <string.h>

static bool VectorFail(VectorFrame *s,const char *message)
{
    s->failed=true;s->event=VECTOR_ERROR;s->note=message;return false;
}
void VectorReset(VectorFrame *s,const int *values,int size,int capacity)
{
    *s=(VectorFrame){.active=-1,.it=-1,.line="Ready",.note="Choose an answer to start the example."};
    if(size<0 || size>capacity || capacity>VECTOR_LIMIT || capacity<0 || (size && !values)) {
        VectorFail(s,"Invalid model size or capacity.");return;
    }
    s->size=size;s->capacity=capacity;
    for(int i=0;i<size;i++) s->data[i]=values[i];
}
/* 한글: end() 위치도 유효한 반복자지만 역참조할 수는 없다.
 * English: end() is a valid iterator position, but is never dereferenceable. */
bool VectorIterator(VectorFrame *s,int index)
{
    if(s->failed || s->done) return false;
    if(index<0 || index>s->size) return VectorFail(s,"Iterator position is outside begin..end.");
    s->it=index;s->valid=true;s->active=-1;s->event=ITER_MOVE;
    s->line="it = modules.begin() + offset";
    s->note=index==s->size?"it equals end(). Do not dereference it.":"Iterator acquired from the current vector.";
    return true;
}
bool VectorRead(VectorFrame *s)
{
    if(s->failed || s->done) return false;
    s->line="read(*it)";
    if(!s->valid) return VectorFail(s,"Invalid iterator read blocked. Reacquire it or use erase's returned iterator.");
    if(s->it<0 || s->it>=s->size) return VectorFail(s,"end() read blocked. end() is not an element.");
    s->active=s->it;s->readValue=s->data[s->it];s->scanned|=1u<<s->it;
    s->event=ITER_READ;s->note="Read the value through a valid iterator.";return true;
}
bool VectorAdvance(VectorFrame *s)
{
    if(s->failed || s->done) return false;
    s->line="++it";
    if(!s->valid || s->it<0 || s->it>=s->size) return VectorFail(s,"Cannot increment an invalid iterator or end().");
    s->it++;s->event=ITER_MOVE;
    s->note=s->it==s->size?"Reached end(). The scan is finished.":"Iterator moved to the next element.";return true;
}
/* 한글: 이 데모만 capacity를 두 배로 늘린다. STL의 실제 증가 비율은 정해져 있지 않다.
 * English: Only this demo doubles capacity. The STL does not require a growth factor. */
bool VectorPush(VectorFrame *s,int value)
{
    if(s->failed || s->done) return false;
    s->line="modules.push_back(value)";
    if(s->size>=VECTOR_LIMIT) return VectorFail(s,"Teaching model storage limit reached.");
    int oldSize=s->size;
    s->reallocated=s->size==s->capacity;
    if(s->reallocated) {
        int grown=s->capacity?s->capacity*2:1;
        s->capacity=grown>VECTOR_LIMIT?VECTOR_LIMIT:grown;
    }
    if(s->valid && (s->reallocated || s->it==oldSize)) s->valid=false;
    s->data[s->size++]=value;s->active=s->size-1;s->event=VECTOR_PUSH;
    s->note=s->reallocated?"Demo storage grew. All old iterators are now invalid.":"No reallocation. Element iterators survive; the old end() does not.";
    return true;
}
/* 한글: erase 위치와 뒤의 반복자는 무효다. 반환값을 받으면 새 위치로 갱신한다.
 * English: Erase invalidates at/after its position; accepting its return obtains a new iterator. */
bool VectorErase(VectorFrame *s,int index,bool takeReturnedIterator)
{
    if(s->failed || s->done) return false;
    s->line=takeReturnedIterator?"it = modules.erase(it)":"modules.erase(it)";
    if(index<0 || index>=s->size) return VectorFail(s,"erase needs an existing element, not end().");
    for(int i=index;i+1<s->size;i++) s->data[i]=s->data[i+1];
    s->size--;s->data[s->size]=0;s->reallocated=false;s->scanned=0;
    if(s->valid && s->it>=index) s->valid=false;
    if(takeReturnedIterator) {s->it=index;s->valid=true;}
    s->active=index<s->size?index:-1;s->event=VECTOR_ERASE;
    s->note=takeReturnedIterator?(index==s->size?"Erase returned end(). It is valid but cannot be read.":"The returned iterator now points to the following element."):
        "The old iterator at this slot is invalid, even though a new value moved here.";
    return true;
}

/* 한글: 최종 미션은 플레이어의 조작으로 상태가 바뀐다. 정답 배열만 자동 재생하지 않는다.
 * English: Player actions drive the final mission rather than an automatic answer trace. */
bool VectorBoss(VectorFrame *s,BossAction action)
{
    if(s->failed || s->done) return false;
    switch(action) {
        case BOSS_SCAN:
            if(!VectorRead(s)) return false;
            if(s->readValue==99) {s->found=true;s->note="Faulty module 99 found. Use it = erase(it).";return true;}
            if(!VectorAdvance(s)) return false;
            s->note=s->it==s->size?"Scan reached end(). Append the required module 40.":"Read a healthy module, then moved it to the next position.";return true;
        case BOSS_ERASE:
            if(!s->found || !s->valid || s->it<0 || s->it>=s->size || s->data[s->it]!=99)
                return VectorFail(s,"Find module 99 with the iterator before erasing.");
            if(!VectorErase(s,s->it,true)) return false;
            s->removed=true;s->found=false;return true;
        case BOSS_PUSH:
            if(!s->removed || s->pushed || !s->valid || s->it!=s->size)
                return VectorFail(s,"Finish scanning to end() after removing 99, then append once.");
            if(!VectorPush(s,40)) return false;
            s->pushed=true;s->note="Module 40 added. The old end iterator is invalid. Verify without reusing it.";return true;
        case BOSS_CHECK:
            s->event=VECTOR_CHECK;s->line="verify complete circuit";s->done=true;
            s->passed=s->removed && s->pushed && s->size==4 && memcmp(s->data,(int[]){10,20,30,40},4*sizeof(int))==0;
            if(!s->passed) return VectorFail(s,"Incomplete circuit. Reset and follow the repair steps.");
            s->note="BOMB DEFUSED! Final order: 10 -> 20 -> 30 -> 40.";return true;
    }
    return false;
}

static bool Record(VectorLesson *v,const VectorFrame *s)
{
    if(v->n>=VECTOR_FRAMES) return false;
    v->frames[v->n++]=*s;return true;
}
bool VectorLoad(VectorLesson *v,int level,int variant)
{
    if(!v || level<0 || level>2 || variant<0 || variant>2) return false;
    *v=(VectorLesson){.level=level,.answer=-1,.variant=variant};
    VectorFrame s;
    VectorReset(&s,level==0?(int[]){10,20,30}:(int[]){10,99,20,30},level==0?3:4,level==0?3:4);
    Record(v,&s);
    if(level==2) {
        VectorIterator(&s,0);s.note="Scan each module. Stop at 99 and remove it safely.";
        v->frames[0]=s;return true;
    }
    #define SAVE() do {if(!Record(v,&s)) return false;} while(0)
    if(level==0) {
        VectorIterator(&s,0);s.line="auto saved = power.begin()";SAVE();
        for(int value=40;value<=60;value+=10) {VectorPush(&s,value);s.line=value==40?"power.push_back(40)":value==50?"power.push_back(50)":"power.push_back(60)";SAVE();}
        s.done=true;s.passed=s.size==6;s.event=VECTOR_CHECK;s.line="power.size() == 6";
        s.note="Six modules installed. Capacity shown is this model's example, not a fixed STL guarantee.";SAVE();
    } else {
        VectorIterator(&s,variant==2?3:1);SAVE();
        VectorErase(&s,s.it,variant!=1);SAVE();
        VectorRead(&s);SAVE();
        if(!s.failed) {
            s.done=true;s.passed=s.readValue==20;s.event=VECTOR_CHECK;s.line="read(*it) == 20";
            s.note="Iterator rescued: erase returned a valid iterator to module 20.";SAVE();
        }
    }
    #undef SAVE
    return true;
}
bool VectorChoose(VectorLesson *v,int answer)
{
    if(!v || v->answer>=0 || answer<0 || answer>2) return false;
    v->answer=answer;v->playing=true;v->timer=0;return true;
}
void VectorNext(VectorLesson *v)
{
    if(v->answer>=0 && v->step+1<v->n) v->step++;
    if(v->step+1>=v->n) v->playing=false;
}
void VectorBack(VectorLesson *v)
{
    v->playing=false;v->timer=0;if(v->step>0) v->step--;
}
const char *VectorEventName(VectorEvent e)
{
    switch(e) {
        case VECTOR_PUSH:return "VECTOR_PUSH";case VECTOR_ERASE:return "VECTOR_ERASE";
        case ITER_MOVE:return "ITER_MOVE";case ITER_READ:return "ITER_READ";
        case VECTOR_CHECK:return "VERIFY";case VECTOR_ERROR:return "BLOCKED";default:return "READY";
    }
}
const char *VectorCode(int level,int variant)
{
    if(level==0) return "// C++ / initial capacity is implementation-specific\nstd::vector<int> power = {10, 20, 30};\nauto saved = power.begin();\npower.push_back(40);\npower.push_back(50);\npower.push_back(60);\n// power.size() == 6\n// Do not use saved after reallocation.\n// Capacity growth shown is an example.";
    if(level==1 && variant==2) return "// Erasing the last element\nauto it = modules.end() - 1;\nit = modules.erase(it); // returns end()\nread(*it); // INVALID: end is not an element";
    if(level==1 && variant==1) return "std::vector<int> modules = {10, 99, 20, 30};\nauto it = modules.begin() + 1;\nmodules.erase(it);\nread(*it); // INVALID iterator";
    if(level==1) return "std::vector<int> modules = {10, 99, 20, 30};\nauto it = modules.begin() + 1;\nit = modules.erase(it);\nread(*it); // valid: 20";
    return "// C++ / reference repair loop\nfor (auto it = modules.begin();\n     it != modules.end();) {\n    if (*it == 99) {\n        it = modules.erase(it);\n    } else {\n        ++it;\n    }\n}\nmodules.push_back(40);";
}
