#include "vm.h"
#include "../opcode/opcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct Var { char *name; ScmlValue value; };
struct ScmlVM { uint8_t *code; size_t code_size; size_t pc; int trace; ScmlValue stack[SCML_STACK_MAX]; size_t sp; struct Var vars[SCML_VARS_MAX]; size_t var_count; int32_t heap[SCML_HEAP_MAX]; size_t heap_top; ScmlEntity entities[SCML_ENTITIES_MAX]; int next_entity_id; };

static char *xstrdup(const char*s){size_t n=strlen(s);char*r=(char*)malloc(n+1);if(r)memcpy(r,s,n+1);return r;}
static void val_free(ScmlValue*v){ if(v->type==SCML_VAL_STRING) free(v->string); v->string=NULL; }
static ScmlValue val_int(int32_t x){ ScmlValue v={SCML_VAL_INT,x,NULL}; return v; }
static ScmlValue val_str(const char*s){ ScmlValue v={SCML_VAL_STRING,0,xstrdup(s?s:"")}; return v; }
static ScmlValue val_clone(const ScmlValue*v){ return v->type==SCML_VAL_STRING?val_str(v->string):val_int(v->integer); }
static int to_int(const ScmlValue*v){ return v->type==SCML_VAL_INT?v->integer:atoi(v->string?v->string:"0"); }
static const char *to_cstr(const ScmlValue*v,char*buf,size_t n){ if(v->type==SCML_VAL_STRING)return v->string; snprintf(buf,n,"%d",v->integer); return buf; }
static uint8_t r8(ScmlVM*vm){ return vm->code[vm->pc++]; } static uint16_t r16(ScmlVM*vm){ uint16_t v=vm->code[vm->pc]|(vm->code[vm->pc+1]<<8); vm->pc+=2; return v; } static uint32_t r32(ScmlVM*vm){ uint32_t v=0; for(int i=0;i<4;i++)v|=((uint32_t)vm->code[vm->pc++])<<(i*8); return v; }

static struct Var *var_find(ScmlVM*vm,const char*n,int create){ for(size_t i=0;i<vm->var_count;i++)if(strcmp(vm->vars[i].name,n)==0)return &vm->vars[i]; if(create&&vm->var_count<SCML_VARS_MAX){ struct Var*v=&vm->vars[vm->var_count++]; v->name=xstrdup(n); v->value=val_int(0); return v;} return NULL; }
static int push(ScmlVM*vm,ScmlValue v){ if(vm->sp>=SCML_STACK_MAX){val_free(&v);return 0;} vm->stack[vm->sp++]=v; return 1; }

typedef struct DecOp { ScmlOperandType type; int32_t i; char *s; } DecOp;
static void dec_free(DecOp*ops,size_t n){ for(size_t i=0;i<n;i++) if(ops[i].type==SCML_OPERAND_STRING||ops[i].type==SCML_OPERAND_VAR) free(ops[i].s); }
static int decode_operand(ScmlVM*vm,DecOp*o,char*err,size_t err_size){ if(vm->pc>=vm->code_size){snprintf(err,err_size,"truncated operand");return 0;} o->type=(ScmlOperandType)r8(vm); if(o->type==SCML_OPERAND_INT||o->type==SCML_OPERAND_ADDRESS)o->i=(int32_t)r32(vm); else if(o->type==SCML_OPERAND_STRING||o->type==SCML_OPERAND_VAR){ uint16_t n=r16(vm); if(vm->pc+n>vm->code_size){snprintf(err,err_size,"truncated string operand");return 0;} o->s=(char*)malloc(n+1); memcpy(o->s,vm->code+vm->pc,n); o->s[n]=0; vm->pc+=n; } else {snprintf(err,err_size,"bad operand tag %d",o->type);return 0;} return 1; }
static ScmlValue eval(ScmlVM*vm,DecOp*o){ if(o->type==SCML_OPERAND_INT||o->type==SCML_OPERAND_ADDRESS)return val_int(o->i); if(o->type==SCML_OPERAND_STRING)return val_str(o->s); struct Var*v=var_find(vm,o->s,0); return v?val_clone(&v->value):val_int(0); }
static int set_var(ScmlVM*vm,const char*n,ScmlValue v){ struct Var*var=var_find(vm,n,1); if(!var){val_free(&v);return 0;} val_free(&var->value); var->value=v; return 1; }

ScmlVM *scml_vm_create(void){ ScmlVM*vm=(ScmlVM*)calloc(1,sizeof(*vm)); if(vm)vm->next_entity_id=1; return vm; }
void scml_vm_destroy(ScmlVM*vm){ if(!vm)return; free(vm->code); for(size_t i=0;i<vm->var_count;i++){free(vm->vars[i].name);val_free(&vm->vars[i].value);} for(size_t i=0;i<vm->sp;i++)val_free(&vm->stack[i]); free(vm); }
void scml_vm_set_trace(ScmlVM*vm,int e){vm->trace=e;}
int scml_vm_load_file(ScmlVM*vm,const char*path,char*err,size_t err_size){ FILE*f=fopen(path,"rb"); if(!f){snprintf(err,err_size,"cannot open %s",path);return 0;} uint8_t hdr[10]; if(fread(hdr,1,10,f)!=10){snprintf(err,err_size,"bad header");fclose(f);return 0;} uint32_t magic=hdr[0]|(hdr[1]<<8)|(hdr[2]<<16)|(hdr[3]<<24); uint16_t ver=hdr[4]|(hdr[5]<<8); uint32_t size=hdr[6]|(hdr[7]<<8)|(hdr[8]<<16)|(hdr[9]<<24); if(magic!=SCML_MAGIC||ver!=SCML_VERSION){snprintf(err,err_size,"not a supported scmlbin");fclose(f);return 0;} free(vm->code); vm->code=(uint8_t*)malloc(size); vm->code_size=size; vm->pc=0; if(fread(vm->code,1,size,f)!=size){snprintf(err,err_size,"truncated bytecode");fclose(f);return 0;} fclose(f); return 1; }
int scml_vm_set_int(ScmlVM*vm,const char*n,int32_t value){ return set_var(vm,n,val_int(value)); }
int scml_vm_get_int(ScmlVM*vm,const char*n,int32_t*out){ struct Var*v=var_find(vm,n,0); if(!v)return 0; *out=to_int(&v->value); return 1; }
static void wait_ms(int ms){ clock_t end=clock()+(clock_t)((double)ms*CLOCKS_PER_SEC/1000.0); while(clock()<end){} }

int scml_vm_run(ScmlVM*vm,char*err,size_t err_size){ while(vm->pc<vm->code_size){ size_t ins_pc=vm->pc; ScmlOpcode op=(ScmlOpcode)r8(vm); uint8_t argc=r8(vm); DecOp ops[8]; memset(ops,0,sizeof(ops)); if(argc>8){snprintf(err,err_size,"too many operands at %zu",ins_pc);return 0;} for(uint8_t i=0;i<argc;i++) if(!decode_operand(vm,&ops[i],err,err_size)){dec_free(ops,i);return 0;} if(vm->trace) fprintf(stderr,"[SCML] pc=%zu op=%s argc=%u\n",ins_pc,scml_opcode_name(op),argc);
        switch(op){
        case SCML_OP_NOP: break; case SCML_OP_HALT: dec_free(ops,argc); return 1;
        case SCML_OP_PUSH_INT: push(vm,val_int(ops[0].i)); break; case SCML_OP_PUSH_STR: push(vm,val_str(ops[0].s)); break;
        case SCML_OP_LOAD:{ ScmlValue v=eval(vm,&ops[0]); push(vm,v); break; }
        case SCML_OP_STORE:{ ScmlValue v=eval(vm,&ops[1]); if(!set_var(vm,ops[0].s,v)){snprintf(err,err_size,"variable table full");dec_free(ops,argc);return 0;} break; }
        case SCML_OP_ADD: case SCML_OP_SUB: case SCML_OP_MUL: case SCML_OP_DIV:{ ScmlValue a=eval(vm,&ops[1]), b=eval(vm,&ops[2]); int x=to_int(&a), y=to_int(&b), r=0; if(op==SCML_OP_ADD)r=x+y; else if(op==SCML_OP_SUB)r=x-y; else if(op==SCML_OP_MUL)r=x*y; else { if(y==0){snprintf(err,err_size,"division by zero");return 0;} r=x/y;} val_free(&a);val_free(&b); set_var(vm,ops[0].s,val_int(r)); break; }
        case SCML_OP_JMP: vm->pc=(size_t)ops[0].i; break;
        case SCML_OP_IF_EQ: case SCML_OP_IF_NE: case SCML_OP_IF_GT: case SCML_OP_IF_LT:{ ScmlValue a=eval(vm,&ops[0]), b=eval(vm,&ops[1]); int x=to_int(&a), y=to_int(&b), take=0; if(op==SCML_OP_IF_EQ)take=x==y; else if(op==SCML_OP_IF_NE)take=x!=y; else if(op==SCML_OP_IF_GT)take=x>y; else take=x<y; val_free(&a);val_free(&b); if(take)vm->pc=(size_t)ops[2].i; break; }
        case SCML_OP_PRINT: case SCML_OP_LOG:{ ScmlValue v=eval(vm,&ops[0]); char b[64]; printf("%s\n",to_cstr(&v,b,sizeof(b))); val_free(&v); break; }
        case SCML_OP_WAIT:{ ScmlValue v=eval(vm,&ops[0]); wait_ms(to_int(&v)); val_free(&v); break; }
        case SCML_OP_FILE_READ:{ ScmlValue path=eval(vm,&ops[0]); char pb[64]; FILE*f=fopen(to_cstr(&path,pb,sizeof(pb)),"rb"); char *data=xstrdup(""); if(f){fseek(f,0,SEEK_END); long n=ftell(f); rewind(f); data=(char*)realloc(data,(size_t)n+1); if(fread(data,1,(size_t)n,f)!=(size_t)n) data[0]=0; else data[n]=0; fclose(f);} set_var(vm,ops[1].s,val_str(data)); free(data); val_free(&path); break; }
        case SCML_OP_FILE_WRITE:{ ScmlValue path=eval(vm,&ops[0]), data=eval(vm,&ops[1]); char pb[64], db[64]; FILE*f=fopen(to_cstr(&path,pb,sizeof(pb)),"ab"); if(f){fputs(to_cstr(&data,db,sizeof(db)),f); fputc('\n',f); fclose(f);} val_free(&path);val_free(&data); break; }
        case SCML_OP_ENTITY_SPAWN:{ ScmlValue model=eval(vm,&ops[0]),x=eval(vm,&ops[1]),y=eval(vm,&ops[2]),z=eval(vm,&ops[3]); int id=vm->next_entity_id++; for(size_t i=0;i<SCML_ENTITIES_MAX;i++) if(!vm->entities[i].active){vm->entities[i].active=1;vm->entities[i].id=id;snprintf(vm->entities[i].model,sizeof(vm->entities[i].model),"%s",model.type==SCML_VAL_STRING?model.string:"entity");vm->entities[i].x=to_int(&x);vm->entities[i].y=to_int(&y);vm->entities[i].z=to_int(&z);break;} set_var(vm,ops[4].s,val_int(id)); val_free(&model);val_free(&x);val_free(&y);val_free(&z); break; }
        case SCML_OP_ENTITY_SET: break;
        case SCML_OP_HEAP_ALLOC:{ ScmlValue sz=eval(vm,&ops[0]); int n=to_int(&sz); if(n<0||vm->heap_top+(size_t)n>=SCML_HEAP_MAX){snprintf(err,err_size,"heap exhausted");return 0;} set_var(vm,ops[1].s,val_int((int)vm->heap_top)); vm->heap_top+=(size_t)n; val_free(&sz); break; }
        case SCML_OP_HEAP_STORE:{ ScmlValue a=eval(vm,&ops[0]),i=eval(vm,&ops[1]),v=eval(vm,&ops[2]); size_t idx=(size_t)(to_int(&a)+to_int(&i)); if(idx>=SCML_HEAP_MAX){snprintf(err,err_size,"heap store out of range");return 0;} vm->heap[idx]=to_int(&v); val_free(&a);val_free(&i);val_free(&v); break; }
        case SCML_OP_HEAP_LOAD:{ ScmlValue a=eval(vm,&ops[1]),i=eval(vm,&ops[2]); size_t idx=(size_t)(to_int(&a)+to_int(&i)); if(idx>=SCML_HEAP_MAX){snprintf(err,err_size,"heap load out of range");return 0;} set_var(vm,ops[0].s,val_int(vm->heap[idx])); val_free(&a);val_free(&i); break; }
        default: snprintf(err,err_size,"unknown opcode %u at %zu",op,ins_pc); dec_free(ops,argc); return 0; }
        dec_free(ops,argc); }
    return 1; }
