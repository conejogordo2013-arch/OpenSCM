#include "compiler.h"
#include "../parser/parser.h"
#include "../opcode/opcode.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LabelAddr { char *name; uint32_t addr; } LabelAddr;
static void w8(FILE*f,uint8_t v){fputc(v,f);} static void w16(FILE*f,uint16_t v){fputc(v&255,f);fputc(v>>8,f);} static void w32(FILE*f,uint32_t v){for(int i=0;i<4;i++)fputc((v>>(i*8))&255,f);} 
static uint32_t stmt_size(const ScmlStatement*s){ if(s->label)return 0; uint32_t n=2; for(size_t i=0;i<s->operand_count;i++){ n+=1; if(s->operands[i].type==SCML_OPERAND_INT||s->operands[i].type==SCML_OPERAND_ADDRESS)n+=4; else n+=2+(uint32_t)strlen(s->operands[i].text); } return n; }
static LabelAddr *find_label(LabelAddr*l,size_t c,const char*n){ for(size_t i=0;i<c;i++) if(strcmp(l[i].name,n)==0) return &l[i]; return NULL; }
static uint32_t resolve_value(const ScmlOperand*o,LabelAddr*labels,size_t label_count,char*err,size_t err_size){ if(o->type==SCML_OPERAND_ADDRESS){ LabelAddr*l=find_label(labels,label_count,o->text); if(!l){snprintf(err,err_size,"unresolved label @%s",o->text);return UINT32_MAX;} return l->addr;} return (uint32_t)o->integer; }

int scml_compile_file(const char *source_path, const char *output_path, char *err, size_t err_size){ ScmlProgram p; if(!scml_parse_file(source_path,&p,err,err_size))return 0; LabelAddr *labels=NULL; size_t lc=0; uint32_t pc=0; for(size_t i=0;i<p.count;i++){ if(p.items[i].label){ labels=(LabelAddr*)realloc(labels,(lc+1)*sizeof(*labels)); labels[lc].name=p.items[i].label; labels[lc].addr=pc; lc++; } else pc+=stmt_size(&p.items[i]); }
    FILE*f=fopen(output_path,"wb"); if(!f){snprintf(err,err_size,"cannot create %s",output_path); free(labels); scml_program_free(&p); return 0;} w32(f,SCML_MAGIC); w16(f,SCML_VERSION); w32(f,pc); for(size_t i=0;i<p.count;i++){ ScmlStatement*s=&p.items[i]; if(s->label)continue; w8(f,(uint8_t)s->opcode); w8(f,(uint8_t)s->operand_count); for(size_t j=0;j<s->operand_count;j++){ ScmlOperand*o=&s->operands[j]; w8(f,(uint8_t)o->type); if(o->type==SCML_OPERAND_INT||o->type==SCML_OPERAND_ADDRESS){ uint32_t v=resolve_value(o,labels,lc,err,err_size); if(v==UINT32_MAX){fclose(f); remove(output_path); free(labels); scml_program_free(&p); return 0;} w32(f,v); } else { size_t n=strlen(o->text); if(n>65535){snprintf(err,err_size,"string/variable too long");fclose(f);remove(output_path);free(labels);scml_program_free(&p);return 0;} w16(f,(uint16_t)n); fwrite(o->text,1,n,f); } } }
    fclose(f); free(labels); scml_program_free(&p); return 1; }
