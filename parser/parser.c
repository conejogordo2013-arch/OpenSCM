#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include "../lexer/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Macro { char *name; char **args; size_t argc; char *body; struct Macro *next; } Macro;
typedef struct CondFrame { int parent_active; int branch_taken; int active; } CondFrame;

static char *xstrdup(const char *s){ size_t n=strlen(s); char *r=(char*)malloc(n+1); if(r) memcpy(r,s,n+1); return r; }
static char *strndup2(const char *s,size_t n){ char *r=(char*)malloc(n+1); if(r){memcpy(r,s,n);r[n]=0;} return r; }
static void append(char **buf,size_t *len,size_t *cap,const char *s){ size_t n=strlen(s); if(*len+n+1>*cap){*cap=(*len+n+1)*2+128;*buf=(char*)realloc(*buf,*cap);} memcpy(*buf+*len,s,n);*len+=n;(*buf)[*len]=0; }
static char *trim(char *s){ while(isspace((unsigned char)*s))s++; char *e=s+strlen(s); while(e>s&&isspace((unsigned char)e[-1]))*--e=0; return s; }
static int starts(const char *s,const char *p){ return strncmp(s,p,strlen(p))==0; }
static int is_ident_char(char c){ return isalnum((unsigned char)c) || c=='_'; }
static int path_equals(const char *a, const char *b){ return strcmp(a,b)==0; }
static int is_absolute_path(const char *p){
    if(!p||!p[0]) return 0;
    if(p[0]=='/' || p[0]=='\\') return 1;
    if(isalpha((unsigned char)p[0]) && p[1]==':' && (p[2]=='/' || p[2]=='\\')) return 1;
    return 0;
}

static char *read_file(const char *path, char *err, size_t err_size){ FILE *f=fopen(path,"rb"); if(!f){snprintf(err,err_size,"cannot open %s",path);return NULL;} fseek(f,0,SEEK_END); long n=ftell(f); rewind(f); char *b=(char*)malloc((size_t)n+1); if(!b){fclose(f);return NULL;} if(fread(b,1,(size_t)n,f)!=(size_t)n){snprintf(err,err_size,"cannot read %s",path);free(b);fclose(f);return NULL;} b[n]=0; fclose(f); return b; }
static char *dirname_of(const char *p){
    const char *a=strrchr(p,'/');
    const char *b=strrchr(p,'\\');
    const char *s = a>b ? a : b;
    if(!s) return xstrdup(".");
    return strndup2(p,(size_t)(s-p));
}

static int has_trailing_sep(const char *p){
    size_t n=strlen(p);
    if(n==0) return 0;
    char c=p[n-1];
    return c=='/' || c=='\\';
}

static void build_include_path(char *dst, size_t dst_size, const char *base_dir, const char *inc_name){
    if(is_absolute_path(inc_name)){
        snprintf(dst,dst_size,"%s",inc_name);
        return;
    }
    if(strcmp(base_dir,".")==0){
        snprintf(dst,dst_size,"%s",inc_name);
        return;
    }
    snprintf(dst,dst_size, has_trailing_sep(base_dir)?"%s%s":"%s/%s", base_dir, inc_name);
}

static void macro_free(Macro *m){ while(m){Macro*n=m->next; free(m->name); for(size_t i=0;i<m->argc;i++)free(m->args[i]); free(m->args); free(m->body); free(m); m=n;} }
static Macro *find_macro(Macro *m,const char *name){ for(;m;m=m->next) if(strcmp(m->name,name)==0) return m; return NULL; }
static void macro_remove(Macro **head, const char *name){ Macro *prev=NULL,*cur=*head; while(cur){ if(strcmp(cur->name,name)==0){ if(prev) prev->next=cur->next; else *head=cur->next; cur->next=NULL; macro_free(cur); return; } prev=cur; cur=cur->next; } }

static char *replace_all(const char *src,const char *key,const char *val){ char *out=NULL; size_t len=0,cap=0; size_t k=strlen(key); const char *p=src; while(*p){ if((p==src||!(isalnum((unsigned char)p[-1])||p[-1]=='_'))&&strncmp(p,key,k)==0&&!(isalnum((unsigned char)p[k])||p[k]=='_')){ append(&out,&len,&cap,val); p+=k; } else { char tmp[2]={*p++,0}; append(&out,&len,&cap,tmp);} } return out?out:xstrdup(""); }

static char **split_args(char *s,size_t *argc){ char **out=NULL; *argc=0; char *p=s; while(p&&*p){ while(isspace((unsigned char)*p)||*p==',')p++; if(!*p)break; char *start=p; int quoted=0; int depth=0; while(*p && (quoted || depth>0 || *p!=',')){ if(*p=='"') quoted=!quoted; else if(!quoted && *p=='(') depth++; else if(!quoted && *p==')' && depth>0) depth--; p++; } char saved=*p; *p=0; out=(char**)realloc(out,(*argc+1)*sizeof(char*)); out[(*argc)++]=xstrdup(trim(start)); if(saved) p++; } return out; }

static char *stringize(const char *s){ size_t n=strlen(s); char *o=(char*)malloc(n*2+3); size_t j=0; o[j++]='"'; for(size_t i=0;i<n;i++){ if(s[i]=='"' || s[i]=='\\') o[j++]='\\'; o[j++]=s[i]; } o[j++]='"'; o[j]=0; return o; }

static const char *skip_spaces_const(const char *s){ while(*s && isspace((unsigned char)*s)) s++; return s; }
static int parse_line_directive(char *t, int *line_override, char *file_override, size_t file_override_size){
    char *p=trim(t+5);
    if(!*p) return 0;
    char *end=NULL;
    long v=strtol(p,&end,10);
    if(end==p || v<=0) return 0;
    *line_override=(int)v;
    end=trim(end);
    if(*end=='"'){
        char *q=end+1;
        char *r=strchr(q,'"');
        if(!r) return 0;
        size_t n=(size_t)(r-q);
        if(n>=file_override_size) n=file_override_size-1;
        memcpy(file_override,q,n);
        file_override[n]=0;
    } else {
        file_override[0]=0;
    }
    return 1;
}

static char *expand_macro_call(Macro *m, const char *line){
    char *expanded=xstrdup(m->body?m->body:"");
    const char *lp=strchr(line,'(');
    if(!lp) return expanded;
    char *argcopy=xstrdup(lp+1); char *rp=strrchr(argcopy,')'); if(rp) *rp=0;
    size_t ac=0; char **vals=split_args(argcopy,&ac);
    for(size_t i=0;i<m->argc&&i<ac;i++){
        char hashpat[128]; snprintf(hashpat,sizeof(hashpat),"#%s",m->args[i]);
        char *qs=stringize(vals[i]); char *n1=replace_all(expanded,hashpat,qs); free(qs); free(expanded); expanded=n1;
    }
    for(size_t i=0;i<m->argc&&i<ac;i++){
        char pastepat[128]; snprintf(pastepat,sizeof(pastepat),"%s##",m->args[i]);
        char *n2=replace_all(expanded,pastepat,vals[i]); free(expanded); expanded=n2;
        snprintf(pastepat,sizeof(pastepat),"##%s",m->args[i]);
        char *n3=replace_all(expanded,pastepat,vals[i]); free(expanded); expanded=n3;
    }
    for(size_t i=0;i<m->argc&&i<ac;i++){ char *ne=replace_all(expanded,m->args[i],vals[i]); free(expanded); expanded=ne; }
    for(size_t i=0;i<ac;i++) free(vals[i]);
    free(vals);
    free(argcopy);
    return expanded;
}

static int eval_expr_simple(const char *expr, Macro *macros){
    char *t=xstrdup(expr); char *s=trim(t);
    if(starts(s,"defined(")){ char *p=s+8; char *r=strchr(p,')'); if(r)*r=0; int v=find_macro(macros,trim(p))!=NULL; free(t); return v; }
    if(starts(s,"defined ")){ int v=find_macro(macros,trim(s+8))!=NULL; free(t); return v; }
    if(find_macro(macros,s)) { free(t); return 1; }
    long v=strtol(s,NULL,0); free(t); return v!=0;
}

static int strip_block_comments_inplace(char *buf){
    char *src=buf,*dst=buf; int in=0;
    while(*src){
        if(!in && src[0]=='/' && src[1]=='*'){ in=1; src+=2; continue; }
        if(in && src[0]=='*' && src[1]=='/'){ in=0; src+=2; continue; }
        if(!in) *dst++=*src;
        src++;
    }
    *dst=0;
    return !in;
}

static int preprocess_text(const char *path, const char *text, Macro **macros, char **out, char *err, size_t err_size){
    char *dir=dirname_of(path);
    char *copy=xstrdup(text);
    if(!strip_block_comments_inplace(copy)){ snprintf(err,err_size,"unterminated block comment in %s",path); goto fail; }
    char *save=NULL; char *line=strtok_r(copy,"\n",&save); size_t olen=0,ocap=0;
    int in_macro=0; Macro *cur=NULL;
    CondFrame cond[64]; int ctop=0; int active=1;
    static char *pragma_once_paths[1024];
    static size_t pragma_once_count=0;
    int line_no=1;
    int logical_line_no=1;
    char logical_file[1024]; snprintf(logical_file,sizeof(logical_file),"%s",path);
    for(size_t i=0;i<pragma_once_count;i++){ if(path_equals(pragma_once_paths[i], path)){ free(dir); free(copy); return 1; } }
    while(line){
        char *raw=line; char *cpp=strstr(raw,"//"); if(cpp) *cpp=0;
        char *t=trim(raw);
        if(starts(t,"#include")){
            if(active){
                char inc_name[1024]={0};
                char *q=strchr(t,'"'); char *r=q?strchr(q+1,'"'):NULL;
                if(q&&r){ snprintf(inc_name,sizeof(inc_name),"%.*s",(int)(r-q-1),q+1); }
                else{
                    q=strchr(t,'<'); r=q?strchr(q+1,'>'):NULL;
                    if(q&&r){ snprintf(inc_name,sizeof(inc_name),"%.*s",(int)(r-q-1),q+1); }
                }
                if(!inc_name[0]){snprintf(err,err_size,"bad include in %s:%d",path,line_no);goto fail;}
                char inc[1024];
                build_include_path(inc,sizeof(inc),dir,inc_name);
                char *it=read_file(inc,err,err_size); if(!it)goto fail;
                char *processed=NULL; if(!preprocess_text(inc,it,macros,&processed,err,err_size)){free(it);goto fail;}
                append(out,&olen,&ocap,processed?processed:""); free(processed); free(it);
            }
        }
        else if(starts(t,"#define")){
            if(active){
                char *p=trim(t+7);
                char *k=p;
                while(*p && !isspace((unsigned char)*p) && *p!='(') p++;
                char saved=*p;
                if(*p) *p++=0;
                Macro*m=(Macro*)calloc(1,sizeof(*m));
                m->name=xstrdup(k);
                char *body_start=p;
                if(saved!='('){
                    body_start=trim(p);
                    if(*body_start=='('){ saved='('; body_start++; }
                }
                if(saved=='('){
                    char *rp=strchr(body_start,')');
                    if(!rp){snprintf(err,err_size,"bad macro define in %s:%d",path,line_no); goto fail;}
                    *rp=0;
                    m->args=split_args(body_start,&m->argc);
                    m->body=xstrdup(trim(rp+1));
                } else {
                    m->body=xstrdup(trim(p));
                }
                m->next=*macros; *macros=m;
            }
        }
        else if(starts(t,"#undef")){ if(active){ macro_remove(macros, trim(t+6)); } }
        else if(starts(t,"#ifdef")){ if(ctop>=64){snprintf(err,err_size,"conditional nesting too deep in %s:%d",path,line_no);goto fail;} int v=find_macro(*macros,trim(t+6))!=NULL; cond[ctop++] = (CondFrame){active, v, active&&v}; active = cond[ctop-1].active; }
        else if(starts(t,"#ifndef")){ if(ctop>=64){snprintf(err,err_size,"conditional nesting too deep in %s:%d",path,line_no);goto fail;} int v=find_macro(*macros,trim(t+7))==NULL; cond[ctop++] = (CondFrame){active, v, active&&v}; active = cond[ctop-1].active; }
        else if(starts(t,"#if")){ if(ctop>=64){snprintf(err,err_size,"conditional nesting too deep in %s:%d",path,line_no);goto fail;} int v=eval_expr_simple(trim(t+3),*macros); cond[ctop++] = (CondFrame){active, v, active&&v}; active = cond[ctop-1].active; }
        else if(starts(t,"#elif")){ if(ctop==0){snprintf(err,err_size,"#elif without #if in %s:%d",path,line_no);goto fail;} CondFrame *f=&cond[ctop-1]; int v=(!f->branch_taken)&&eval_expr_simple(trim(t+5),*macros); f->active=f->parent_active&&v; if(v) f->branch_taken=1; active=f->active; }
        else if(strcmp(t,"#else")==0){ if(ctop==0){snprintf(err,err_size,"#else without #if in %s:%d",path,line_no);goto fail;} CondFrame *f=&cond[ctop-1]; int v=!f->branch_taken; f->active=f->parent_active&&v; f->branch_taken=1; active=f->active; }
        else if(strcmp(t,"#endif")==0){ if(ctop==0){snprintf(err,err_size,"#endif without #if in %s:%d",path,line_no);goto fail;} ctop--; active = (ctop==0)?1:cond[ctop-1].active; }
        else if(starts(t,"#error")){ if(active){ snprintf(err,err_size,"#error %s (%s:%d)",trim(t+6),path,line_no); goto fail; } }
        else if(starts(t,"#warning")){ if(active){ fprintf(stderr,"SCML preprocessor warning: %s (%s:%d)\n",trim(t+8),path,line_no); } }
        else if(starts(t,"#line")){
            if(active){
                int new_line=0;
                char new_file[1024]={0};
                if(!parse_line_directive(t,&new_line,new_file,sizeof(new_file))){
                    snprintf(err,err_size,"bad #line in %s:%d",path,line_no); goto fail;
                }
                logical_line_no=new_line;
                if(new_file[0]) snprintf(logical_file,sizeof(logical_file),"%s",new_file);
            }
        }
        else if(starts(t,"#pragma")){
            if(active){
                char *pr=trim(t+7);
                if(strcmp(pr,"once")==0 && pragma_once_count<1024){
                    pragma_once_paths[pragma_once_count++]=xstrdup(path);
                }
            }
        }
        else if(starts(t,"macro ")){ if(active){ char *p=trim(t+6); char *lp=strchr(p,'('); char *rp=lp?strchr(lp,')'):NULL; char *colon=rp?strchr(rp,':'):NULL; if(!lp||!rp||!colon){snprintf(err,err_size,"bad macro header in %s",path);goto fail;} cur=(Macro*)calloc(1,sizeof(*cur)); cur->name=strndup2(p,(size_t)(lp-p)); char *argtext=strndup2(lp+1,(size_t)(rp-lp-1)); cur->args=split_args(argtext,&cur->argc); free(argtext); in_macro=1; } }
        else if(in_macro && strcmp(t,"endmacro")==0){ cur->next=*macros; *macros=cur; cur=NULL; in_macro=0; }
        else if(in_macro){ size_t bl=cur->body?strlen(cur->body):0, bc=bl+strlen(t)+3; cur->body=(char*)realloc(cur->body,bc); sprintf(cur->body+bl,"%s\n",t); }
        else if(active && *t){
            char tmpDate[64],tmpTime[64]; time_t now=time(NULL); struct tm *tmv=localtime(&now); strftime(tmpDate,sizeof(tmpDate),"%b %d %Y",tmv); strftime(tmpTime,sizeof(tmpTime),"%H:%M:%S",tmv);
            char lnBuf[32]; snprintf(lnBuf,sizeof(lnBuf),"%d",logical_line_no);
            char *lineout=xstrdup(t);
            char *n=replace_all(lineout,"__FILE__",logical_file); free(lineout); lineout=n;
            n=replace_all(lineout,"__LINE__",lnBuf); free(lineout); lineout=n;
            n=replace_all(lineout,"__DATE__",tmpDate); free(lineout); lineout=n;
            n=replace_all(lineout,"__TIME__",tmpTime); free(lineout); lineout=n;
            n=replace_all(lineout,"__scml","1"); free(lineout); lineout=n;

            const char *head=lineout;
            while(*head && isspace((unsigned char)*head)) head++;
            char namebuf[128]; size_t ni=0;
            while(head[ni] && is_ident_char(head[ni]) && ni+1<sizeof(namebuf)){ namebuf[ni]=head[ni]; ni++; }
            namebuf[ni]=0;
            const char *after_name=skip_spaces_const(head+ni);
            Macro*m=find_macro(*macros,namebuf);
            if(m && m->argc>0 && *after_name=='('){ char *expanded=expand_macro_call(m,head); free(lineout); lineout=expanded; }
            for(Macro*d=*macros;d;d=d->next) if(d->argc==0){ char *ne=replace_all(lineout,d->name,d->body?d->body:""); free(lineout); lineout=ne; }
            append(out,&olen,&ocap,lineout); append(out,&olen,&ocap,"\n"); free(lineout);
        }
        line=strtok_r(NULL,"\n",&save); line_no++; logical_line_no++;
    }
    if(ctop!=0){ snprintf(err,err_size,"unterminated conditional block in %s",path); goto fail; }
    free(dir); free(copy); return 1;
fail:
    free(dir); free(copy); return 0;
}

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
