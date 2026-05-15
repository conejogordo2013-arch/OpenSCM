#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include "../lexer/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Macro { char *name; char **args; size_t argc; char *body; struct Macro *next; } Macro;

static char *xstrdup(const char *s){ size_t n=strlen(s); char *r=(char*)malloc(n+1); if(r) memcpy(r,s,n+1); return r; }
static char *strndup2(const char *s,size_t n){ char *r=(char*)malloc(n+1); if(r){memcpy(r,s,n);r[n]=0;} return r; }
static void append(char **buf,size_t *len,size_t *cap,const char *s){ size_t n=strlen(s); if(*len+n+1>*cap){*cap=(*len+n+1)*2+128;*buf=(char*)realloc(*buf,*cap);} memcpy(*buf+*len,s,n);*len+=n;(*buf)[*len]=0; }
static char *trim(char *s){ while(isspace((unsigned char)*s))s++; char *e=s+strlen(s); while(e>s&&isspace((unsigned char)e[-1]))*--e=0; return s; }
static int starts(const char *s,const char *p){ return strncmp(s,p,strlen(p))==0; }

static char *read_file(const char *path, char *err, size_t err_size){ FILE *f=fopen(path,"rb"); if(!f){snprintf(err,err_size,"cannot open %s",path);return NULL;} fseek(f,0,SEEK_END); long n=ftell(f); rewind(f); char *b=(char*)malloc((size_t)n+1); if(!b){fclose(f);return NULL;} if(fread(b,1,(size_t)n,f)!=(size_t)n){snprintf(err,err_size,"cannot read %s",path);free(b);fclose(f);return NULL;} b[n]=0; fclose(f); return b; }
static char *dirname_of(const char *p){ const char *s=strrchr(p,'/'); if(!s) return xstrdup("."); return strndup2(p,(size_t)(s-p)); }

static void macro_free(Macro *m){ while(m){Macro*n=m->next; free(m->name); for(size_t i=0;i<m->argc;i++)free(m->args[i]); free(m->args); free(m->body); free(m); m=n;} }

static Macro *find_macro(Macro *m,const char *name){ for(;m;m=m->next) if(strcmp(m->name,name)==0) return m; return NULL; }

static char *replace_all(const char *src,const char *key,const char *val){ char *out=NULL; size_t len=0,cap=0; size_t k=strlen(key); const char *p=src; while(*p){ if((p==src||!isalnum((unsigned char)p[-1]))&&strncmp(p,key,k)==0&&!isalnum((unsigned char)p[k])){ append(&out,&len,&cap,val); p+=k; } else { char tmp[2]={*p++,0}; append(&out,&len,&cap,tmp);} } return out?out:xstrdup(""); }

static char **split_args(char *s,size_t *argc){ char **out=NULL; *argc=0; char *p=s; while(p&&*p){ while(isspace((unsigned char)*p)||*p==',')p++; if(!*p)break; char *start=p; int quoted=0; while(*p&&(quoted||*p!=',')){ if(*p=='"')quoted=!quoted; p++; } char saved=*p; *p=0; out=(char**)realloc(out,(*argc+1)*sizeof(char*)); out[(*argc)++]=xstrdup(trim(start)); if(saved) p++; } return out; }

static int preprocess_text(const char *path, const char *text, Macro **macros, char **out, char *err, size_t err_size){ char *dir=dirname_of(path); char *copy=xstrdup(text); char *save=NULL; char *line=strtok_r(copy,"\n",&save); size_t olen=0,ocap=0; int in_macro=0; Macro *cur=NULL; while(line){ char *t=trim(line); if(starts(t,"#include")){ char *q=strchr(t,'"'); char *r=q?strchr(q+1,'"'):NULL; if(!q||!r){snprintf(err,err_size,"bad include in %s",path);goto fail;} char inc[1024]; snprintf(inc,sizeof(inc),"%s/%.*s",dir,(int)(r-q-1),q+1); char *it=read_file(inc,err,err_size); if(!it)goto fail; char *processed=NULL; if(!preprocess_text(inc,it,macros,&processed,err,err_size)){free(it);goto fail;} if(processed){ append(out,&olen,&ocap,processed); free(processed); } free(it); }
        else if(starts(t,"#define")){ char *p=trim(t+7); char *k=p; while(*p&&!isspace((unsigned char)*p))p++; if(*p)*p++=0; Macro*m=(Macro*)calloc(1,sizeof(*m)); m->name=xstrdup(k); m->body=xstrdup(trim(p)); m->next=*macros; *macros=m; }
        else if(starts(t,"macro ")){ char *p=trim(t+6); char *lp=strchr(p,'('); char *rp=lp?strchr(lp,')'):NULL; char *colon=rp?strchr(rp,':'):NULL; if(!lp||!rp||!colon){snprintf(err,err_size,"bad macro header in %s",path);goto fail;} cur=(Macro*)calloc(1,sizeof(*cur)); cur->name=strndup2(p,(size_t)(lp-p)); char *argtext=strndup2(lp+1,(size_t)(rp-lp-1)); cur->args=split_args(argtext,&cur->argc); free(argtext); in_macro=1; }
        else if(in_macro && strcmp(t,"endmacro")==0){ cur->next=*macros; *macros=cur; cur=NULL; in_macro=0; }
        else if(in_macro){ size_t bl=cur->body?strlen(cur->body):0, bc=bl+strlen(t)+3; cur->body=(char*)realloc(cur->body,bc); sprintf(cur->body+bl,"%s\n",t); }
        else if(*t){ char namebuf[128]; size_t ni=0; while(t[ni]&&!isspace((unsigned char)t[ni])&&t[ni]!='('&&ni+1<sizeof(namebuf)){namebuf[ni]=t[ni];ni++;} namebuf[ni]=0; Macro*m=find_macro(*macros,namebuf); if(m){ char *expanded=xstrdup(m->body?m->body:""); if(m->argc){ char *argcopy=xstrdup(t); char *argline=strchr(argcopy,'('); if(!argline) argline=strchr(argcopy,' '); if(argline){ if(*argline=='('){argline++; char *rp=strrchr(argline,')'); if(rp)*rp=0;} size_t ac=0; char **vals=split_args(argline,&ac); for(size_t i=0;i<m->argc&&i<ac;i++){ char *ne=replace_all(expanded,m->args[i],vals[i]); free(expanded); expanded=ne; } for(size_t i=0;i<ac;i++)free(vals[i]); free(vals); } free(argcopy); } for(Macro*d=*macros;d;d=d->next) if(d->argc==0){ char *ne=replace_all(expanded,d->name,d->body?d->body:""); free(expanded); expanded=ne; } append(out,&olen,&ocap,expanded); if(strlen(expanded)==0||expanded[strlen(expanded)-1]!='\n')append(out,&olen,&ocap,"\n"); free(expanded); } else { char *lineout=xstrdup(t); for(Macro*d=*macros;d;d=d->next) if(d->argc==0){ char *ne=replace_all(lineout,d->name,d->body?d->body:""); free(lineout); lineout=ne; } append(out,&olen,&ocap,lineout); append(out,&olen,&ocap,"\n"); free(lineout); } }
        line=strtok_r(NULL,"\n",&save); }
    free(dir); free(copy); return 1; fail: free(dir); free(copy); return 0; }

int scml_preprocess_file(const char *path, char **out_text, char *err, size_t err_size){ Macro *macros=NULL; char *txt=read_file(path,err,err_size); if(!txt)return 0; *out_text=NULL; int ok=preprocess_text(path,txt,&macros,out_text,err,err_size); free(txt); macro_free(macros); return ok; }

static int add_stmt(ScmlProgram*p,ScmlStatement*s){ if(p->count==p->capacity){size_t nc=p->capacity?p->capacity*2:32; ScmlStatement*ni=(ScmlStatement*)realloc(p->items,nc*sizeof(*ni)); if(!ni)return 0; p->items=ni;p->capacity=nc;} p->items[p->count++]=*s; return 1; }
static int parse_int(const char*s){ if(strlen(s)>1&&s[0]=='0'&&isxdigit((unsigned char)s[1])) return (int)strtol(s,NULL,16); return (int)strtol(s,NULL,0); }
static int is_float_token(const char*s){ if(*s=='-')s++; int dot=0,digit=0; while(*s){ if(*s=='.'){ if(dot)return 0; dot=1; } else if(isdigit((unsigned char)*s)) digit=1; else return 0; s++; } return dot&&digit; }
static int is_number_token(const char*s){ if(*s=='-')s++; if(!*s)return 0; while(*s){ if(!isxdigit((unsigned char)*s))return 0; s++; } return 1; }

int scml_parse_file(const char *path, ScmlProgram *program, char *err, size_t err_size){ memset(program,0,sizeof(*program)); char *txt=NULL; if(!scml_preprocess_file(path,&txt,err,err_size))return 0; char *save=NULL,*line=strtok_r(txt,"\n",&save); int ln=1; while(line){ ScmlTokenList toks; if(!scml_lex_line(line,ln,&toks,err,err_size)){free(txt);return 0;} if(toks.count){ ScmlStatement st; memset(&st,0,sizeof(st)); st.line=ln; size_t idx=0; if(toks.items[0].type==SCML_TOK_COLON && toks.count>1){ st.label=xstrdup(toks.items[1].text); if(!add_stmt(program,&st)){free(txt);return 0;} idx=2; }
            if(idx<toks.count){ memset(&st,0,sizeof(st)); st.line=ln; const ScmlOpcodeInfo *info=NULL; char *op=toks.items[idx].text; int assign = (idx + 2 < toks.count && strcmp(toks.items[idx + 1].text, "=") == 0); int plus_assign = (idx + 2 < toks.count && strcmp(toks.items[idx + 1].text, "+=") == 0); int minus_assign = (idx + 2 < toks.count && strcmp(toks.items[idx + 1].text, "-=") == 0); if(assign||plus_assign||minus_assign){ info=scml_opcode_from_name(assign?"SET":(plus_assign?"ADD":"SUB")); st.opcode=info->opcode; ScmlOperand *dst=&st.operands[st.operand_count++]; dst->type=SCML_OPERAND_VAR; dst->text=xstrdup(toks.items[idx].text); if(!assign){ ScmlOperand *src=&st.operands[st.operand_count++]; src->type=SCML_OPERAND_VAR; src->text=xstrdup(toks.items[idx].text); } ScmlToken *tk=&toks.items[idx+2]; ScmlOperand *val=&st.operands[st.operand_count++]; val->text=xstrdup(tk->text); if(tk->type==SCML_TOK_STRING)val->type=SCML_OPERAND_STRING; else if(tk->type==SCML_TOK_LABEL_REF)val->type=SCML_OPERAND_ADDRESS; else if(tk->type==SCML_TOK_NUMBER && is_float_token(tk->text)){val->type=SCML_OPERAND_FLOAT;val->real=(float)strtod(tk->text,NULL);} else if(tk->type==SCML_TOK_NUMBER){val->type=SCML_OPERAND_INT;val->integer=parse_int(tk->text);} else val->type=SCML_OPERAND_VAR; }
            else { size_t oplen=strlen(op); if(oplen&&op[oplen-1]==':')op[--oplen]=0; if(is_number_token(op)) info=scml_opcode_from_scm_code((uint16_t)strtol(op,NULL,16)); else info=scml_opcode_from_name(op); if(!info){snprintf(err,err_size,"line %d: unknown opcode '%s'",ln,op);free(txt);return 0;} st.opcode=info->opcode; if(idx+1<toks.count&&toks.items[idx+1].type==SCML_TOK_COLON)idx++; idx++; for(;idx<toks.count && st.operand_count<8;idx++){ ScmlToken *tk=&toks.items[idx]; ScmlOperand *o=&st.operands[st.operand_count++]; o->text=xstrdup(tk->text); if(tk->type==SCML_TOK_STRING)o->type=SCML_OPERAND_STRING; else if(tk->type==SCML_TOK_LABEL_REF)o->type=SCML_OPERAND_ADDRESS; else if(tk->type==SCML_TOK_NUMBER && is_float_token(tk->text)){o->type=SCML_OPERAND_FLOAT;o->real=(float)strtod(tk->text,NULL);} else if(tk->type==SCML_TOK_NUMBER){o->type=SCML_OPERAND_INT;o->integer=parse_int(tk->text);} else o->type=SCML_OPERAND_VAR; } }
            if(st.operand_count<info->min_args||st.operand_count>info->max_args){snprintf(err,err_size,"line %d: %s expects %u..%u args",ln,info->name,info->min_args,info->max_args);free(txt);return 0;} if(!add_stmt(program,&st)){free(txt);return 0;} } }
        scml_token_list_free(&toks); line=strtok_r(NULL,"\n",&save); ln++; }
    free(txt); return 1; }

void scml_program_free(ScmlProgram *p){ for(size_t i=0;i<p->count;i++){ free(p->items[i].label); for(size_t j=0;j<p->items[i].operand_count;j++)free(p->items[i].operands[j].text);} free(p->items); memset(p,0,sizeof(*p)); }
