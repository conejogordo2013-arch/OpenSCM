#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include "../lexer/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Macro { char *name; char **args; size_t argc; int function_like; char *body; struct Macro *next; } Macro;
typedef struct CondFrame { int parent_active; int branch_taken; int active; } CondFrame;

typedef enum ModernBlockKind { MODERN_BLOCK_SCOPE, MODERN_BLOCK_SKIP, MODERN_BLOCK_SCRIPT, MODERN_BLOCK_FUNCTION, MODERN_BLOCK_TASK, MODERN_BLOCK_IF, MODERN_BLOCK_ELSE, MODERN_BLOCK_ELSEIF, MODERN_BLOCK_WHILE, MODERN_BLOCK_FOR } ModernBlockKind;
typedef struct ModernBlock { ModernBlockKind kind; char start_label[96]; char false_label[96]; char end_label[96]; char continue_label[96]; char post[512]; } ModernBlock;


static char *xstrdup(const char *s){ size_t n=strlen(s); char *r=(char*)malloc(n+1); if(r) memcpy(r,s,n+1); return r; }
static char *strndup2(const char *s,size_t n){ char *r=(char*)malloc(n+1); if(r){memcpy(r,s,n);r[n]=0;} return r; }
static void append(char **buf,size_t *len,size_t *cap,const char *s){ size_t n=strlen(s); if(*len+n+1>*cap){*cap=(*len+n+1)*2+128;*buf=(char*)realloc(*buf,*cap);} memcpy(*buf+*len,s,n);*len+=n;(*buf)[*len]=0; }
static char *trim(char *s){ while(isspace((unsigned char)*s))s++; char *e=s+strlen(s); while(e>s&&isspace((unsigned char)e[-1]))*--e=0; return s; }
static int starts(const char *s,const char *p){ return strncmp(s,p,strlen(p))==0; }
static int path_equals(const char *a, const char *b){ return strcmp(a,b)==0; }
static int file_exists(const char *path){ FILE *f=fopen(path,"rb"); if(!f) return 0; fclose(f); return 1; }
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

static void resolve_include_path(char *dst, size_t dst_size, const char *base_dir, const char *inc_name){
    build_include_path(dst, dst_size, base_dir, inc_name);
    if(file_exists(dst) || is_absolute_path(inc_name)) return;
    char root_candidate[1024];
    build_include_path(root_candidate, sizeof(root_candidate), ".", inc_name);
    if(file_exists(root_candidate)){ snprintf(dst,dst_size,"%s",root_candidate); return; }
    const char *paths = getenv("SCML_PATH");
    if(!paths || !*paths) return;
    char *copy = xstrdup(paths);
    if(!copy) return;
    char *save = NULL;
    char *entry = strtok_r(copy, ":;", &save);
    while(entry){
        char *dir = trim(entry);
        if(*dir){
            build_include_path(dst, dst_size, dir, inc_name);
            if(file_exists(dst)){ free(copy); return; }
        }
        entry = strtok_r(NULL, ":;", &save);
    }
    free(copy);
    build_include_path(dst, dst_size, base_dir, inc_name);
}


static char **split_args(char *s,size_t *argc);
static void append_line(char **out, size_t *olen, size_t *ocap, const char *line);
static int is_keyword_boundary(char c){ return c==0 || isspace((unsigned char)c) || c=='(' || c=='{' || c==':' || c=='<' || c=='['; }
static int starts_keyword(const char *s, const char *kw){ size_t n=strlen(kw); return strncmp(s,kw,n)==0 && is_keyword_boundary(s[n]); }
static int starts_keyword_ci(const char *s, const char *kw){
    size_t n=strlen(kw);
    for(size_t i=0;i<n;i++){
        if(!s[i]) return 0;
        if(tolower((unsigned char)s[i])!=tolower((unsigned char)kw[i])) return 0;
    }
    return is_keyword_boundary(s[n]);
}
static int equals_ci(const char *a, const char *b){
    while(*a && *b){ if(tolower((unsigned char)*a)!=tolower((unsigned char)*b)) return 0; a++; b++; }
    return *a==0 && *b==0;
}
static void strip_statement_tail(char *s){ char *t=trim(s); size_t n=strlen(t); while(n>0 && isspace((unsigned char)t[n-1])) t[--n]=0; if(n>0 && t[n-1]==';') t[--n]=0; while(n>0 && isspace((unsigned char)t[n-1])) t[--n]=0; }
static char *strip_modern_attributes(char *s){
    char *t=trim(s);
    while(starts(t,"[[")){
        char *end=strstr(t,"]] ");
        if(!end) end=strstr(t,"]]");
        if(!end) break;
        t=trim(end+2);
    }
    return t;
}
static void sanitize_label_name(char *s){ for(size_t i=0;s[i];i++){ if(!isalnum((unsigned char)s[i]) && s[i]!='_') s[i]='_'; } }
static char *read_modern_ident(char *s, char *out, size_t out_size){ s=trim(s); size_t i=0; while(s[i] && (isalnum((unsigned char)s[i]) || s[i]=='_' || s[i]=='.' || s[i]==':' || s[i]=='$')){ if(i+1<out_size) out[i]=s[i]; i++; } if(out_size) out[i<out_size?i:out_size-1]=0; return s+i; }
static char *find_unquoted_binary_op(char *s, const char *op){
    int quoted=0, depth=0;
    size_t n=strlen(op);
    for(char *p=s; *p; p++){
        if(*p=='"' && (p==s || p[-1]!='\\')) quoted=!quoted;
        else if(!quoted && (*p=='(' || *p=='[' || *p=='{')) depth++;
        else if(!quoted && (*p==')' || *p==']' || *p=='}') && depth>0) depth--;
        else if(!quoted && depth==0 && strncmp(p,op,n)==0) return p;
    }
    return NULL;
}
static char *find_top_level_char(char *s, char needle){
    int quoted=0, depth=0;
    for(char *p=s; *p; p++){
        if(*p=='"' && (p==s || p[-1]!='\\')) quoted=!quoted;
        else if(!quoted && (*p=='(' || *p=='[' || *p=='{')) depth++;
        else if(!quoted && (*p==')' || *p==']' || *p=='}') && depth>0) depth--;
        else if(!quoted && depth==0 && *p==needle) return p;
    }
    return NULL;
}
static void normalize_type_name(char *s){
    for(size_t i=0;s[i];i++){ if(s[i]==' ' || s[i]=='	') s[i]='_'; else s[i]=(char)tolower((unsigned char)s[i]); }
    if(strcmp(s,"int")==0) snprintf(s,256,"i32");
    else if(strcmp(s,"float")==0) snprintf(s,256,"f32");
    else if(strcmp(s,"double")==0) snprintf(s,256,"f64");
    else if(strcmp(s,"string")==0) snprintf(s,256,"str");
}
static int modern_emit_value_assignment(const char *name, char *value, char **out, size_t *olen, size_t *ocap){
    char *val=trim(value);
    char outl[768];
    static const struct { const char *op; const char *code; } ops[]={{"+","0006"},{"-","0007"},{"*","0008"},{"/","0009"},{"%","0B07"}};
    for(size_t i=0;i<sizeof(ops)/sizeof(ops[0]);i++){
        char scratch[768]; snprintf(scratch,sizeof(scratch),"%s",val);
        char *p=find_unquoted_binary_op(scratch,ops[i].op);
        if(p && p!=scratch){ *p=0; char *lhs=trim(scratch); char *rhs=trim(p+strlen(ops[i].op)); if(*lhs && *rhs){ snprintf(outl,sizeof(outl),"%s: %s %s %s",ops[i].code,name,lhs,rhs); append_line(out,olen,ocap,outl); return 1; } }
    }
    if((starts(val,"fold_add(") || starts(val,"fold_mul(") || starts(val,"fold_any(") || starts(val,"fold_all(")) && val[strlen(val)-1]==')'){
        int is_mul=starts(val,"fold_mul(");
        int is_any=starts(val,"fold_any(");
        int is_all=starts(val,"fold_all(");
        char argsbuf[768];
        const char *lp=strchr(val,'(');
        snprintf(argsbuf,sizeof(argsbuf),"%.*s",(int)(strlen(lp+1)-1),lp+1);
        size_t argc=0; char **args=split_args(argsbuf,&argc);
        if(argc==0){ snprintf(outl,sizeof(outl),"0004: %s %d",name,is_mul||is_all?1:0); append_line(out,olen,ocap,outl); }
        else {
            snprintf(outl,sizeof(outl),"0004: %s %s",name,trim(args[0])); append_line(out,olen,ocap,outl);
            for(size_t i=1;i<argc;i++){
                const char *code=is_mul?"0008":(is_any?"0B28":(is_all?"0B27":"0006"));
                snprintf(outl,sizeof(outl),"%s: %s %s %s",code,name,name,trim(args[i])); append_line(out,olen,ocap,outl);
            }
        }
        for(size_t i=0;i<argc;i++) free(args[i]);
        free(args);
        return 1;
    }
    if((starts(val,"consteval_add(") || starts(val,"consteval_sub(") || starts(val,"consteval_mul(") || starts(val,"consteval_div(")) && val[strlen(val)-1]==')'){
        char argsbuf[768]; const char *lp=strchr(val,'(');
        snprintf(argsbuf,sizeof(argsbuf),"%.*s",(int)(strlen(lp+1)-1),lp+1);
        size_t argc=0; char **args=split_args(argsbuf,&argc);
        if(argc!=2){ for(size_t i=0;i<argc;i++){ free(args[i]); } free(args); snprintf(outl,sizeof(outl),"0004: %s 0",name); append_line(out,olen,ocap,outl); return 1; }
        const char *code=starts(val,"consteval_sub(")?"0007":(starts(val,"consteval_mul(")?"0008":(starts(val,"consteval_div(")?"0009":"0006"));
        snprintf(outl,sizeof(outl),"%s: %s %s %s",code,name,trim(args[0]),trim(args[1])); append_line(out,olen,ocap,outl);
        for(size_t i=0;i<argc;i++){ free(args[i]); } free(args); return 1;
    }
    if(starts(val,"range_size(") && val[strlen(val)-1]==')'){
        char ref[256]; const char *lp=strchr(val,'('); snprintf(ref,sizeof(ref),"%.*s",(int)(strlen(lp+1)-1),lp+1);
        snprintf(outl,sizeof(outl),"0B12: 98@ %s 0",trim(ref)); append_line(out,olen,ocap,outl);
        snprintf(outl,sizeof(outl),"0B12: 99@ %s 1",trim(ref)); append_line(out,olen,ocap,outl);
        snprintf(outl,sizeof(outl),"0007: %s 99@ 98@",name); append_line(out,olen,ocap,outl);
        return 1;
    }
    snprintf(outl,sizeof(outl),"0004: %s %s",name,val);
    append_line(out,olen,ocap,outl);
    return 1;
}
static int modern_emit_typed_declaration(char *type, char *decl, char **out, size_t *olen, size_t *ocap, char *err, size_t err_size){
    strip_statement_tail(decl);
    char *eq=find_unquoted_binary_op(decl,"=");
    char *name=decl;
    char *value=NULL;
    if(eq){ *eq=0; value=trim(eq+1); }
    name=trim(name);
    while(*name=='*' || *name=='&') name++;
    name=trim(name);
    if(!*name){ snprintf(err,err_size,"modern typed declaration expects a variable name"); return 0; }
    char typebuf[256]; snprintf(typebuf,sizeof(typebuf),"%s",trim(type)); normalize_type_name(typebuf);
    char outl[512]; snprintf(outl,sizeof(outl),"0B4B: %s \"%s\"",name,typebuf); append_line(out,olen,ocap,outl);
    if(value && *value) modern_emit_value_assignment(name,value,out,olen,ocap);
    else { snprintf(outl,sizeof(outl),"0004: %s 0",name); append_line(out,olen,ocap,outl); }
    return 1;
}
static int modern_emit_auto_declaration(char *decl, char **out, size_t *olen, size_t *ocap, ModernBlock *blocks, int *block_top, char *err, size_t err_size){
    char *p=trim(decl);
    if(*p=='['){
        char *rb=strchr(p,']'); char *eq=rb?find_unquoted_binary_op(rb+1,"="):NULL;
        if(!rb || !eq){ snprintf(err,err_size,"modern structured binding expects AUTO [a, b] = value"); return 0; }
        *rb=0;
        char *items=p+1;
        char *source=trim(eq+1);
        size_t argc=0; char **names=split_args(items,&argc);
        for(size_t i=0;i<argc;i++){
            char *name=trim(names[i]);
            if(*name){
                char outl[256];
                snprintf(outl,sizeof(outl),"0B4B: %s \"any\"",name); append_line(out,olen,ocap,outl);
                if(source[0]=='$' || strchr(source,'@')) snprintf(outl,sizeof(outl),"0B12: %s %s %zu",name,source,i);
                else snprintf(outl,sizeof(outl),"0004: %s 0",name);
                append_line(out,olen,ocap,outl);
            }
            free(names[i]);
        }
        free(names);
        return 1;
    }
    char *eq=find_unquoted_binary_op(p,"=");
    if(!eq){ return modern_emit_typed_declaration("any",p,out,olen,ocap,err,err_size); }
    *eq=0;
    char *name=trim(p); char *value=trim(eq+1);
    if(!*name){ snprintf(err,err_size,"modern AUTO declaration expects a variable name"); return 0; }
    if(starts(value,"[]") || starts(value,"[&]") || starts(value,"[=]")){
        char outl[512]; snprintf(outl,sizeof(outl),"0B4B: %s \"any\"",name); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),"0004: %s 0",name); append_line(out,olen,ocap,outl);
        if(strchr(value,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_SKIP,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    char outl[512]; snprintf(outl,sizeof(outl),"0B4B: %s \"any\"",name); append_line(out,olen,ocap,outl);
    return modern_emit_value_assignment(name,value,out,olen,ocap);
}
static int modern_type_keyword_len(const char *t){
    static const char *kw[]={"INT","I32","I64","U32","FLOAT","F32","DOUBLE","F64","STRING","STR","BOOL","VOID","VECTOR","MAP","LIST","AUTO","SCML::VECTOR","SCML::STRING","SCML::MAP","SCML::LIST","SCML::OPTIONAL","SCML::VARIANT","SCML::ANY","SCML::EXPECTED","SCML::UNORDERED_MAP","SCML::FILESYS::PATH","THREAD","CONSTEXPR"};
    for(size_t i=0;i<sizeof(kw)/sizeof(kw[0]);i++) if(starts_keyword_ci(t,kw[i])) return (int)strlen(kw[i]);
    return 0;
}
static int find_condition_op(char *expr, char **lhs, char **rhs, const char **opcode);
static int split_for_clauses(char *header, char **init, char **cond, char **post){
    int quoted=0;
    int depth=0;
    char *parts[3]={0};
    int part=0;
    parts[part]=header;
    for(char *c=header; *c; c++){
        if(*c=='"' && (c==header || c[-1]!='\\')) quoted=!quoted;
        else if(!quoted && (*c=='(' || *c=='[' || *c=='{')) depth++;
        else if(!quoted && (*c==')' || *c==']' || *c=='}') && depth>0) depth--;
        else if(!quoted && depth==0 && *c==';'){
            if(part>=2) return 0;
            *c=0;
            parts[++part]=c+1;
        }
    }
    if(part!=2) return 0;
    *init=trim(parts[0]);
    *cond=trim(parts[1]);
    *post=trim(parts[2]);
    return 1;
}
static int split_condition_initializer(char *condition, char **init, char **cond){
    int quoted=0;
    int depth=0;
    *init=NULL;
    *cond=trim(condition);
    for(char *c=condition; *c; c++){
        if(*c=='"' && (c==condition || c[-1]!='\\')) quoted=!quoted;
        else if(!quoted && (*c=='(' || *c=='[' || *c=='{')) depth++;
        else if(!quoted && (*c==')' || *c==']' || *c=='}') && depth>0) depth--;
        else if(!quoted && depth==0 && *c==';'){
            *c=0;
            *init=trim(condition);
            *cond=trim(c+1);
            return **init && **cond;
        }
    }
    return 1;
}

static int modern_eval_const_condition(char *expr, int *out_value){
    char tmp[512];
    snprintf(tmp,sizeof(tmp),"%s",trim(expr));
    if(strcmp(tmp,"true")==0 || strcmp(tmp,"TRUE")==0 || strcmp(tmp,"1")==0){ *out_value=1; return 1; }
    if(strcmp(tmp,"false")==0 || strcmp(tmp,"FALSE")==0 || strcmp(tmp,"0")==0){ *out_value=0; return 1; }
    char *lhs=NULL,*rhs=NULL; const char *opcode=NULL;
    if(!find_condition_op(tmp,&lhs,&rhs,&opcode)) return 0;
    char *end=NULL; long a=strtol(lhs,&end,0); if(end==lhs || *trim(end)) return 0;
    end=NULL; long b=strtol(rhs,&end,0); if(end==rhs || *trim(end)) return 0;
    if(strcmp(opcode,"00D6")==0) *out_value=(a==b);
    else if(strcmp(opcode,"00D7")==0) *out_value=(a!=b);
    else if(strcmp(opcode,"0B25")==0) *out_value=(a>=b);
    else if(strcmp(opcode,"0B26")==0) *out_value=(a<=b);
    else if(strcmp(opcode,"00D8")==0) *out_value=(a>b);
    else if(strcmp(opcode,"00D9")==0) *out_value=(a<b);
    else return 0;
    return 1;
}
static const char *compound_assignment_opcode(const char *op){
    if(!op) return NULL;
    if(strcmp(op,"+=")==0) return "ADD";
    if(strcmp(op,"-=")==0) return "SUB";
    if(strcmp(op,"*=")==0) return "MUL";
    if(strcmp(op,"/=")==0) return "DIV";
    if(strcmp(op,"%=")==0) return "MOD";
    if(strcmp(op,"&=")==0) return "BIT_AND";
    if(strcmp(op,"|=")==0) return "BIT_OR";
    if(strcmp(op,"^=")==0) return "BIT_XOR";
    if(strcmp(op,"<<=")==0) return "SHL";
    if(strcmp(op,">>=")==0) return "SHR";
    return NULL;
}
static int find_condition_op(char *expr, char **lhs, char **rhs, const char **opcode){
    static const struct { const char *op; const char *code; } ops[]={{"==","00D6"},{"!=","00D7"},{">=","0B25"},{"<=","0B26"},{">","00D8"},{"<","00D9"}};
    for(size_t i=0;i<sizeof(ops)/sizeof(ops[0]);i++){
        char *p=strstr(expr,ops[i].op);
        if(p){ *p=0; *lhs=trim(expr); *rhs=trim(p+strlen(ops[i].op)); *opcode=ops[i].code; return **lhs && **rhs; }
    }
    return 0;
}
static void append_line(char **out, size_t *olen, size_t *ocap, const char *line){ append(out,olen,ocap,line); append(out,olen,ocap,"\n"); }
static int append_modern_call_args(char **out, size_t *olen, size_t *ocap, const char *prefix, char *args_text){
    append(out,olen,ocap,prefix);
    size_t argc=0; char **args=split_args(args_text,&argc);
    for(size_t i=0;i<argc;i++){ append(out,olen,ocap," "); append(out,olen,ocap,trim(args[i])); free(args[i]); }
    free(args);
    append(out,olen,ocap,"\n");
    return 1;
}
static int modern_emit_close(ModernBlock *blocks, int *block_top, char **out, size_t *olen, size_t *ocap, char *err, size_t err_size){
    if(*block_top<=0) return 1;
    ModernBlock b=blocks[--(*block_top)];
    if(b.kind==MODERN_BLOCK_SCRIPT) append_line(out,olen,ocap,"0001:");
    else if(b.kind==MODERN_BLOCK_FUNCTION) append_line(out,olen,ocap,"0D01:");
    else if(b.kind==MODERN_BLOCK_TASK) append_line(out,olen,ocap,"0D02:");
    else if(b.kind==MODERN_BLOCK_IF) { char line[128]; snprintf(line,sizeof(line),":%s",b.false_label); append_line(out,olen,ocap,line); }
    else if(b.kind==MODERN_BLOCK_ELSE) { char line[128]; snprintf(line,sizeof(line),":%s",b.end_label); append_line(out,olen,ocap,line); }
    else if(b.kind==MODERN_BLOCK_ELSEIF) { char line[160]; snprintf(line,sizeof(line),":%s",b.false_label); append_line(out,olen,ocap,line); snprintf(line,sizeof(line),":%s",b.end_label); append_line(out,olen,ocap,line); }
    else if(b.kind==MODERN_BLOCK_WHILE) { char line[160]; snprintf(line,sizeof(line),"000A: @%s",b.start_label); append_line(out,olen,ocap,line); snprintf(line,sizeof(line),":%s",b.end_label); append_line(out,olen,ocap,line); }
    else if(b.kind==MODERN_BLOCK_FOR) { char line[640]; snprintf(line,sizeof(line),":%s",b.continue_label); append_line(out,olen,ocap,line); if(b.post[0]) append_line(out,olen,ocap,b.post); snprintf(line,sizeof(line),"000A: @%s",b.start_label); append_line(out,olen,ocap,line); snprintf(line,sizeof(line),":%s",b.end_label); append_line(out,olen,ocap,line); }
    (void)err; (void)err_size;
    return 1;
}
static int modern_translate_line(char *lineout, ModernBlock *blocks, int *block_top, int *modern_id, char **out, size_t *olen, size_t *ocap, char *err, size_t err_size);

static int modern_emit_inline_statement(const char *statement, ModernBlock *blocks, int *block_top, int *modern_id, char **out, size_t *olen, size_t *ocap, char *err, size_t err_size){
    char tmp[512];
    snprintf(tmp,sizeof(tmp),"%s",statement ? statement : "");
    strip_statement_tail(tmp);
    if(!*trim(tmp)) return 1;
    return modern_translate_line(tmp, blocks, block_top, modern_id, out, olen, ocap, err, err_size);
}

static ModernBlock *modern_find_loop(ModernBlock *blocks, int block_top){
    for(int i=block_top-1;i>=0;i--){
        if(blocks[i].kind==MODERN_BLOCK_WHILE || blocks[i].kind==MODERN_BLOCK_FOR) return &blocks[i];
    }
    return NULL;
}

static int modern_translate_line(char *lineout, ModernBlock *blocks, int *block_top, int *modern_id, char **out, size_t *olen, size_t *ocap, char *err, size_t err_size){
    char *t=strip_modern_attributes(lineout);
    strip_statement_tail(t);
    if(!*t || t[0]==';'){ append_line(out,olen,ocap,t); return 1; }
    if(*block_top>0 && blocks[*block_top-1].kind==MODERN_BLOCK_SKIP){
        if(strcmp(t,"}")==0) return modern_emit_close(blocks,block_top,out,olen,ocap,err,err_size);
        if(strchr(t,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_SKIP,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    size_t tn=strlen(t);
    if(tn>2 && strcmp(t+tn-2,"++")==0){
        t[tn-2]=0;
        char *name=trim(t);
        if(!*name){ snprintf(err,err_size,"modern ++ expects a variable"); return 0; }
        char outl[256]; snprintf(outl,sizeof(outl),"0006: %s %s 1",name,name);
        append_line(out,olen,ocap,outl);
        return 1;
    }
    if(tn>2 && strcmp(t+tn-2,"--")==0){
        t[tn-2]=0;
        char *name=trim(t);
        if(!*name){ snprintf(err,err_size,"modern -- expects a variable"); return 0; }
        char outl[256]; snprintf(outl,sizeof(outl),"0007: %s %s 1",name,name);
        append_line(out,olen,ocap,outl);
        return 1;
    }
    if(strcmp(t,"}")==0) return modern_emit_close(blocks,block_top,out,olen,ocap,err,err_size);
    if(equals_ci(t,"public:") || equals_ci(t,"private:") || equals_ci(t,"protected:") || starts_keyword_ci(t,"PUBLIC") || starts_keyword_ci(t,"PRIVATE") || starts_keyword_ci(t,"PROTECTED")) return 1;
    if(starts(t,"} else")){
        if(*block_top<=0 || (blocks[*block_top-1].kind!=MODERN_BLOCK_IF && blocks[*block_top-1].kind!=MODERN_BLOCK_ELSEIF)){ snprintf(err,err_size,"modern else without matching if"); return 0; }
        ModernBlock prev=blocks[--(*block_top)];
        char *else_tail=trim(t+6);
        char line[192]; snprintf(line,sizeof(line),"000A: @%s",prev.end_label); append_line(out,olen,ocap,line); snprintf(line,sizeof(line),":%s",prev.false_label); append_line(out,olen,ocap,line);
        if(*block_top>=64){ snprintf(err,err_size,"modern block nesting too deep"); return 0; }
        if(starts_keyword(else_tail,"if")){
            char *lp=strchr(else_tail,'('), *rp=strrchr(else_tail,')');
            if(!lp || !rp || rp<=lp){snprintf(err,err_size,"modern else if expects condition in parentheses");return 0;}
            char cond[512]; snprintf(cond,sizeof(cond),"%.*s",(int)(rp-lp-1),lp+1);
            char *init=NULL,*real_cond=NULL; if(!split_condition_initializer(cond,&init,&real_cond)){snprintf(err,err_size,"modern else if initializer expects init; condition");return 0;}
            if(init && *init && !modern_emit_inline_statement(init,blocks,block_top,modern_id,out,olen,ocap,err,err_size)) return 0;
            char *lhs=NULL,*rhs=NULL; const char *code=NULL;
            if(!find_condition_op(real_cond,&lhs,&rhs,&code)){snprintf(err,err_size,"modern else if condition expects == != > < >= <=");return 0;}
            int id=++(*modern_id);
            ModernBlock next={MODERN_BLOCK_ELSEIF,{0},{0},{0},{0},{0}};
            snprintf(next.start_label,sizeof(next.start_label),"__SCMLM_ELSEIF_TRUE_%d",id);
            snprintf(next.false_label,sizeof(next.false_label),"__SCMLM_ELSEIF_FALSE_%d",id);
            snprintf(next.end_label,sizeof(next.end_label),"%s",prev.end_label);
            snprintf(line,sizeof(line),"%s: %s %s @%s",code,lhs,rhs,next.start_label); append_line(out,olen,ocap,line);
            snprintf(line,sizeof(line),"000A: @%s",next.false_label); append_line(out,olen,ocap,line);
            snprintf(line,sizeof(line),":%s",next.start_label); append_line(out,olen,ocap,line);
            blocks[(*block_top)++]=next;
            return 1;
        }
        ModernBlock next={MODERN_BLOCK_ELSE,{0},{0},{0},{0},{0}}; snprintf(next.end_label,sizeof(next.end_label),"%s",prev.end_label); blocks[(*block_top)++]=next; return 1;
    }
    if(starts_keyword_ci(t,"TMPL") || starts_keyword_ci(t,"template") || starts_keyword_ci(t,"concept")){
        if(strchr(t,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_SKIP,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    if(starts_keyword_ci(t,"enum class") || starts_keyword_ci(t,"enum")){
        if(strchr(t,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_SKIP,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    if((starts_keyword_ci(t,"virtual") || starts_keyword_ci(t,"inline") || starts_keyword_ci(t,"static")) && strchr(t,'(') && strchr(t,')')) return 1;
    if(starts_keyword_ci(t,"VOID") && strchr(t,'(') && strchr(t,')')) return 1;
    if(starts_keyword_ci(t,"script")){
        char name[96]={0}; read_modern_ident(t+(starts_keyword_ci(t,"script")?6:6),name,sizeof(name)); if(!name[0]) snprintf(name,sizeof(name),"MAIN"); sanitize_label_name(name); char line[128]; snprintf(line,sizeof(line),":%s",name); append_line(out,olen,ocap,line);
        if(strchr(t,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_SCRIPT,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    if(starts_keyword_ci(t,"import")) return 1;
    if(starts_keyword_ci(t,"module") || starts_keyword_ci(t,"namespace") || starts_keyword_ci(t,"class") || starts_keyword_ci(t,"interface") || starts_keyword_ci(t,"struct")){
        if(strchr(t,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_SCOPE,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    if(starts_keyword_ci(t,"fn") || starts_keyword_ci(t,"function")){
        char *p=t+(starts_keyword_ci(t,"fn")?2:8); char name[96]={0}; read_modern_ident(p,name,sizeof(name)); if(!name[0]){snprintf(err,err_size,"modern function missing name");return 0;} sanitize_label_name(name); char line[128]; snprintf(line,sizeof(line),":%s",name); append_line(out,olen,ocap,line);
        if(strchr(t,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_FUNCTION,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    if(starts_keyword_ci(t,"task")){
        char name[96]={0}; read_modern_ident(t+4,name,sizeof(name)); if(!name[0]){snprintf(err,err_size,"modern task missing name");return 0;} sanitize_label_name(name); char line[128]; snprintf(line,sizeof(line),":%s",name); append_line(out,olen,ocap,line);
        if(strchr(t,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_TASK,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    if(starts_keyword_ci(t,"if")){
        int is_constexpr=0;
        char *head=trim(t+2);
        if(starts_keyword_ci(head,"constexpr")){ is_constexpr=1; head=trim(head+9); }
        char *lp=strchr(head,'('), *rp=strrchr(head,')'); if(!lp || !rp || rp<=lp){snprintf(err,err_size,"modern if expects condition in parentheses");return 0;} char cond[512]; snprintf(cond,sizeof(cond),"%.*s",(int)(rp-lp-1),lp+1);
        char *init=NULL,*real_cond=NULL; if(!split_condition_initializer(cond,&init,&real_cond)){snprintf(err,err_size,"modern if initializer expects init; condition");return 0;}
        if(init && *init && !modern_emit_inline_statement(init,blocks,block_top,modern_id,out,olen,ocap,err,err_size)) return 0;
        if(is_constexpr){
            int keep=0; char constexpr_cond[512]; snprintf(constexpr_cond,sizeof(constexpr_cond),"%s",real_cond);
            if(!modern_eval_const_condition(constexpr_cond,&keep)){snprintf(err,err_size,"modern if constexpr expects literal true/false or integer comparison");return 0;}
            if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;}
            blocks[(*block_top)++]=(ModernBlock){keep?MODERN_BLOCK_SCOPE:MODERN_BLOCK_SKIP,{0},{0},{0},{0},{0}};
            return 1;
        }
        char *lhs=NULL,*rhs=NULL; const char *code=NULL; if(!find_condition_op(real_cond,&lhs,&rhs,&code)){snprintf(err,err_size,"modern if condition expects == != > < >= <=");return 0;}
        int id=++(*modern_id); ModernBlock b={MODERN_BLOCK_IF,{0},{0},{0},{0},{0}}; snprintf(b.start_label,sizeof(b.start_label),"__SCMLM_IF_TRUE_%d",id); snprintf(b.false_label,sizeof(b.false_label),"__SCMLM_IF_FALSE_%d",id); snprintf(b.end_label,sizeof(b.end_label),"__SCMLM_IF_END_%d",id);
        char outl[256]; snprintf(outl,sizeof(outl),"%s: %s %s @%s",code,lhs,rhs,b.start_label); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),"000A: @%s",b.false_label); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),":%s",b.start_label); append_line(out,olen,ocap,outl);
        if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=b; return 1;
    }
    if(starts_keyword_ci(t,"while")){
        char *lp=strchr(t,'('), *rp=strrchr(t,')'); if(!lp || !rp || rp<=lp){snprintf(err,err_size,"modern while expects condition in parentheses");return 0;} char cond[512]; snprintf(cond,sizeof(cond),"%.*s",(int)(rp-lp-1),lp+1); char *lhs=NULL,*rhs=NULL; const char *code=NULL; if(!find_condition_op(cond,&lhs,&rhs,&code)){snprintf(err,err_size,"modern while condition expects == != > < >= <=");return 0;}
        int id=++(*modern_id); ModernBlock b={MODERN_BLOCK_WHILE,{0},{0},{0},{0},{0}}; snprintf(b.start_label,sizeof(b.start_label),"__SCMLM_WHILE_START_%d",id); snprintf(b.false_label,sizeof(b.false_label),"__SCMLM_WHILE_BODY_%d",id); snprintf(b.end_label,sizeof(b.end_label),"__SCMLM_WHILE_END_%d",id);
        char outl[256]; snprintf(outl,sizeof(outl),":%s",b.start_label); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),"%s: %s %s @%s",code,lhs,rhs,b.false_label); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),"000A: @%s",b.end_label); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),":%s",b.false_label); append_line(out,olen,ocap,outl);
        if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=b; return 1;
    }
    if(starts_keyword_ci(t,"for")){
        char *lp=strchr(t,'('), *rp=strrchr(t,')');
        if(!lp || !rp || rp<=lp){snprintf(err,err_size,"modern for expects for(init; condition; post) or for(AUTO item : range)");return 0;}
        char header[1024]; snprintf(header,sizeof(header),"%.*s",(int)(rp-lp-1),lp+1);
        char range_header[1024]; snprintf(range_header,sizeof(range_header),"%s",header);
        char *colon=find_top_level_char(range_header,':');
        char *semicolon=find_top_level_char(range_header,';');
        if(colon && !semicolon){
            *colon=0; char *decl=trim(range_header); char *range_ref=trim(colon+1);
            if(!*decl || !*range_ref){snprintf(err,err_size,"modern range-for expects for(AUTO item : range)");return 0;}
            if(starts_keyword_ci(decl,"AUTO")) decl=trim(decl+4);
            else { int tk=modern_type_keyword_len(decl); if(tk>0) decl=trim(decl+tk); }
            while(*decl=='&' || *decl=='*'){ decl++; } decl=trim(decl);
            if(!*decl){snprintf(err,err_size,"modern range-for missing loop variable");return 0;}
            int id=++(*modern_id); ModernBlock b={MODERN_BLOCK_FOR,{0},{0},{0},{0},{0}};
            snprintf(b.start_label,sizeof(b.start_label),"__SCMLM_RANGE_START_%d",id);
            snprintf(b.false_label,sizeof(b.false_label),"__SCMLM_RANGE_BODY_%d",id);
            snprintf(b.end_label,sizeof(b.end_label),"__SCMLM_RANGE_END_%d",id);
            snprintf(b.continue_label,sizeof(b.continue_label),"__SCMLM_RANGE_CONTINUE_%d",id);
            snprintf(b.post,sizeof(b.post),"0006: __SCMLM_RANGE_IT_%d __SCMLM_RANGE_IT_%d 1",id,id);
            char outl[512];
            snprintf(outl,sizeof(outl),"0B12: __SCMLM_RANGE_IT_%d %s 0",id,range_ref); append_line(out,olen,ocap,outl);
            snprintf(outl,sizeof(outl),"0B12: __SCMLM_RANGE_ENDVAL_%d %s 1",id,range_ref); append_line(out,olen,ocap,outl);
            snprintf(outl,sizeof(outl),":%s",b.start_label); append_line(out,olen,ocap,outl);
            snprintf(outl,sizeof(outl),"00D9: __SCMLM_RANGE_IT_%d __SCMLM_RANGE_ENDVAL_%d @%s",id,id,b.false_label); append_line(out,olen,ocap,outl);
            snprintf(outl,sizeof(outl),"000A: @%s",b.end_label); append_line(out,olen,ocap,outl);
            snprintf(outl,sizeof(outl),":%s",b.false_label); append_line(out,olen,ocap,outl);
            snprintf(outl,sizeof(outl),"0B4B: %s \"any\"",decl); append_line(out,olen,ocap,outl);
            snprintf(outl,sizeof(outl),"0004: %s __SCMLM_RANGE_IT_%d",decl,id); append_line(out,olen,ocap,outl);
            if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=b; return 1;
        }
        char *init=NULL,*cond=NULL,*post=NULL;
        if(!split_for_clauses(header,&init,&cond,&post)){snprintf(err,err_size,"modern for expects exactly three ';'-separated clauses");return 0;}
        if(*init && !modern_emit_inline_statement(init,blocks,block_top,modern_id,out,olen,ocap,err,err_size)) return 0;
        char condcopy[512]; snprintf(condcopy,sizeof(condcopy),"%s",*cond?cond:"1 == 1");
        char *lhs=NULL,*rhs=NULL; const char *code=NULL;
        if(!find_condition_op(condcopy,&lhs,&rhs,&code)){snprintf(err,err_size,"modern for condition expects == != > < >= <=");return 0;}
        int id=++(*modern_id); ModernBlock b={MODERN_BLOCK_FOR,{0},{0},{0},{0},{0}};
        snprintf(b.start_label,sizeof(b.start_label),"__SCMLM_FOR_START_%d",id);
        snprintf(b.false_label,sizeof(b.false_label),"__SCMLM_FOR_BODY_%d",id);
        snprintf(b.end_label,sizeof(b.end_label),"__SCMLM_FOR_END_%d",id);
        snprintf(b.continue_label,sizeof(b.continue_label),"__SCMLM_FOR_CONTINUE_%d",id);
        if(*post){
            char *scratch=NULL; size_t slen=0, scap=0;
            if(!modern_emit_inline_statement(post,blocks,block_top,modern_id,&scratch,&slen,&scap,err,err_size)){ free(scratch); return 0; }
            snprintf(b.post,sizeof(b.post),"%s",scratch?scratch:"");
            size_t plen=strlen(b.post); while(plen>0 && (b.post[plen-1]=='\n' || b.post[plen-1]=='\r')) b.post[--plen]=0;
            free(scratch);
        }
        char outl[256]; snprintf(outl,sizeof(outl),":%s",b.start_label); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),"%s: %s %s @%s",code,lhs,rhs,b.false_label); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),"000A: @%s",b.end_label); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),":%s",b.false_label); append_line(out,olen,ocap,outl);
        if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=b; return 1;
    }
    if(starts_keyword(t,"let") || starts_keyword(t,"var") || starts_keyword(t,"const")){
        char *p=t+(starts_keyword(t,"let")?3:(starts_keyword(t,"var")?3:5)); p=trim(p); char *eq=strchr(p,'='); if(!eq){snprintf(err,err_size,"modern declaration expects =");return 0;} *eq=0; char *decl=trim(p); char *val=trim(eq+1); char *colon=strchr(decl,':'); if(colon){ *colon=0; char *name=trim(decl); char *type=trim(colon+1); char outl[512]; snprintf(outl,sizeof(outl),"0B4B: %s \"%s\"",name,type); append_line(out,olen,ocap,outl); modern_emit_value_assignment(name,val,out,olen,ocap); } else { modern_emit_value_assignment(decl,val,out,olen,ocap); } return 1;
    }
    if(starts_keyword_ci(t,"AUTO")){
        char *p=t+4;
        return modern_emit_auto_declaration(p,out,olen,ocap,blocks,block_top,err,err_size);
    }
    {
        char *decl_start=t;
        int advanced_decl=1;
        while(advanced_decl){
            advanced_decl=0;
            if(starts_keyword_ci(decl_start,"inline")){ decl_start=trim(decl_start+6); advanced_decl=1; }
            if(starts_keyword_ci(decl_start,"static")){ decl_start=trim(decl_start+6); advanced_decl=1; }
            if(starts_keyword_ci(decl_start,"constinit")){ decl_start=trim(decl_start+9); advanced_decl=1; }
            if(starts_keyword_ci(decl_start,"consteval")){ decl_start=trim(decl_start+9); advanced_decl=1; }
            if(starts_keyword_ci(decl_start,"const")){ decl_start=trim(decl_start+5); advanced_decl=1; }
            if(starts_keyword_ci(decl_start,"constexpr")){ decl_start=trim(decl_start+9); advanced_decl=1; }
        }
        int tklen=modern_type_keyword_len(decl_start);
        if(tklen>0){
            char *after=decl_start+tklen;
            if(*after=='<'){
                int depth=0;
                while(*after){ if(*after=='<') depth++; else if(*after=='>'){ depth--; if(depth==0){ after++; break; } } after++; }
            }
            char typebuf[256]; snprintf(typebuf,sizeof(typebuf),"%.*s",(int)(after-decl_start),decl_start);
            return modern_emit_typed_declaration(typebuf,after,out,olen,ocap,err,err_size);
        }
    }
    if(starts_keyword_ci(t,"throw")){
        char *p=t+(starts_keyword_ci(t,"throw")?5:5); p=trim(p); if(!*p){snprintf(err,err_size,"modern throw expects a value");return 0;} char outl[512]; snprintf(outl,sizeof(outl),"0004: $SCML_EXCEPTION %s",p); append_line(out,olen,ocap,outl); snprintf(outl,sizeof(outl),"0004: $SCML_EXCEPTION_ACTIVE 1"); append_line(out,olen,ocap,outl); return 1;
    }
    if(starts_keyword_ci(t,"try") || starts_keyword_ci(t,"catch") || starts_keyword_ci(t,"finally")){
        if(strchr(t,'{')){ if(*block_top>=64){snprintf(err,err_size,"modern block nesting too deep");return 0;} blocks[(*block_top)++]=(ModernBlock){MODERN_BLOCK_SCOPE,{0},{0},{0},{0},{0}}; }
        return 1;
    }
    if(starts_keyword(t,"print") || starts_keyword(t,"log") || starts_keyword(t,"wait")){
        char *lp=strchr(t,'('), *rp=strrchr(t,')');
        if(lp && rp && rp>lp){
            const char *code=starts_keyword(t,"print")?"03E5":(starts_keyword(t,"log")?"03E6":"000B");
            char arg[512]; snprintf(arg,sizeof(arg),"%.*s",(int)(rp-lp-1),lp+1);
            char outl[640]; snprintf(outl,sizeof(outl),"%s: %s",code,trim(arg));
            append_line(out,olen,ocap,outl);
            return 1;
        }
    }
    if(starts_keyword_ci(t,"requires")){
        char *lp=strchr(t,'('), *rp=strrchr(t,')'); char *arrow=strstr(t,"->");
        if(!lp || !rp || rp<=lp || !arrow){snprintf(err,err_size,"modern requires expects requires(value, type) -> out_ok");return 0;}
        char argsbuf[512]; snprintf(argsbuf,sizeof(argsbuf),"%.*s",(int)(rp-lp-1),lp+1);
        size_t argc=0; char **args=split_args(argsbuf,&argc);
        if(argc<2){ for(size_t i=0;i<argc;i++){ free(args[i]); } free(args); snprintf(err,err_size,"modern requires expects value and type"); return 0; }
        char *outvar=trim(arrow+2); char outl[640]; snprintf(outl,sizeof(outl),"0B4C: %s %s %s",trim(args[0]),trim(args[1]),outvar); append_line(out,olen,ocap,outl);
        for(size_t i=0;i<argc;i++){ free(args[i]); } free(args); return 1;
    }
    if(starts_keyword_ci(t,"co_await")){
        char *p=trim(t+8); char *arrow=strstr(p,"->"); if(!arrow){snprintf(err,err_size,"modern co_await expects task -> out_done");return 0;} *arrow=0; char *outvar=trim(arrow+2); char outl[256]; snprintf(outl,sizeof(outl),"0B4A: %s %s",trim(p),outvar); append_line(out,olen,ocap,outl); return 1;
    }
    if(starts_keyword_ci(t,"co_return")){
        char *p=trim(t+9); if(*p) modern_emit_value_assignment("$RETVAL",p,out,olen,ocap); append_line(out,olen,ocap,"0D02:"); return 1;
    }
    if(starts_keyword(t,"spawn")){
        char *p=trim(t+5); char *arrow=strstr(p,"->"); if(!arrow){snprintf(err,err_size,"modern spawn expects -> out_task");return 0;} *arrow=0; char target[96]; snprintf(target,sizeof(target),"%s",trim(p)); sanitize_label_name(target); char *outvar=trim(arrow+2); char outl[256]; snprintf(outl,sizeof(outl),"0B49: @%s %s",target,outvar); append_line(out,olen,ocap,outl); return 1;
    }
    if(starts_keyword_ci(t,"return")){
        char *p=trim(t+6);
        if(*p) modern_emit_value_assignment("$RETVAL",p,out,olen,ocap);
        append_line(out,olen,ocap,"0D01:");
        return 1;
    }
    if(strcmp(t,"halt")==0){ append_line(out,olen,ocap,"0001:"); return 1; }
    if(strcmp(t,"yield")==0){ append_line(out,olen,ocap,"000B: 0"); return 1; }
    if(strcmp(t,"break")==0){ ModernBlock *loop=modern_find_loop(blocks,*block_top); if(!loop){snprintf(err,err_size,"modern break outside loop");return 0;} char outl[160]; snprintf(outl,sizeof(outl),"000A: @%s",loop->end_label); append_line(out,olen,ocap,outl); return 1; }
    if(strcmp(t,"continue")==0){ ModernBlock *loop=modern_find_loop(blocks,*block_top); if(!loop){snprintf(err,err_size,"modern continue outside loop");return 0;} const char *target=loop->kind==MODERN_BLOCK_FOR?loop->continue_label:loop->start_label; char outl[160]; snprintf(outl,sizeof(outl),"000A: @%s",target); append_line(out,olen,ocap,outl); return 1; }
    if(starts_keyword(t,"goto")){ char *label=trim(t+4); char outl[160]; snprintf(outl,sizeof(outl),"000A: %s%s",label[0]=='@'?"":"@",label); append_line(out,olen,ocap,outl); return 1; }
    if(starts_keyword(t,"call")){ char *label=trim(t+4); char *lp=strchr(label,'('); if(lp) *lp=0; label=trim(label); char outl[160]; snprintf(outl,sizeof(outl),"0D00: %s%s",label[0]=='@'?"":"@",label); append_line(out,olen,ocap,outl); return 1; }
    char *arrow=strstr(t,"->"); char *lp=strchr(t,'('); char *rp=strrchr(t,')'); if(lp && rp && rp>lp && strchr(t,'.') && lp>t){ char callee[160]; snprintf(callee,sizeof(callee),"%.*s",(int)(lp-t),t); char args[768]; snprintf(args,sizeof(args),"%.*s",(int)(rp-lp-1),lp+1); char prefix[256]; snprintf(prefix,sizeof(prefix),"0B31: \"%s\"",trim(callee)); append_modern_call_args(out,olen,ocap,prefix,args); if(arrow){ char *outvar=trim(arrow+2); if(*outvar){ char outl[256]; snprintf(outl,sizeof(outl),"0004: %s $RETVAL",outvar); append_line(out,olen,ocap,outl); } } return 1; }
    append_line(out,olen,ocap,t); return 1;
}
static int modern_include_name(char *t, char *inc_name, size_t inc_size){
    if(!starts_keyword(t,"use")) return 0;
    char *p=trim(t+3); strip_statement_tail(p);
    size_t n=strlen(p);
    int explicit_path = 0;
    if(n>=2 && ((p[0]=='"' && p[n-1]=='"') || (p[0]=='<' && p[n-1]=='>'))){ p[n-1]=0; p++; explicit_path = 1; }
    if(!*p) return 0;
    snprintf(inc_name,inc_size,"%s",p);
    size_t ln=strlen(inc_name);
    int has_scml_ext = (ln>=6 && strcmp(inc_name+ln-6,".scmlh")==0) || (ln>=5 && strcmp(inc_name+ln-5,".scml")==0);
    if(!explicit_path && !has_scml_ext) for(size_t i=0;inc_name[i];i++) if(inc_name[i]=='.') inc_name[i]='/';
    if(!has_scml_ext){
        ln=strlen(inc_name);
        snprintf(inc_name+ln,inc_size-ln,".scmlh");
    }
    return 1;
}


static void macro_push_object(Macro **head, const char *name, const char *body){
    if(!name || !*name) return;
    Macro *m=(Macro*)calloc(1,sizeof(*m));
    if(!m) return;
    m->name=xstrdup(name);
    m->body=xstrdup(body?body:"1");
    m->function_like=0;
    m->next=*head;
    *head=m;
}

static void seed_env_defines(Macro **macros){
    const char *env=getenv("SCML_DEFINES");
    if(!env || !*env) return;
    char *copy=xstrdup(env);
    char *save=NULL;
    char *entry=strtok_r(copy,";",&save);
    while(entry){
        char *t=trim(entry);
        if(*t){
            char *name=t;
            while(*t && !isspace((unsigned char)*t) && *t!='=') t++;
            char sep=*t;
            if(*t) *t++=0;
            char *body=trim(t);
            if(sep=='=' && *body=='=') body++;
            macro_push_object(macros,name,*body?body:"1");
        }
        entry=strtok_r(NULL,";",&save);
    }
    free(copy);
}

static void macro_free(Macro *m){ while(m){Macro*n=m->next; free(m->name); for(size_t i=0;i<m->argc;i++)free(m->args[i]); free(m->args); free(m->body); free(m); m=n;} }
static Macro *find_macro(Macro *m,const char *name){ for(;m;m=m->next) if(strcmp(m->name,name)==0) return m; return NULL; }
static void macro_remove(Macro **head, const char *name){ Macro *prev=NULL,*cur=*head; while(cur){ if(strcmp(cur->name,name)==0){ if(prev) prev->next=cur->next; else *head=cur->next; cur->next=NULL; macro_free(cur); return; } prev=cur; cur=cur->next; } }

static char *replace_all(const char *src,const char *key,const char *val){ char *out=NULL; size_t len=0,cap=0; size_t k=strlen(key); if(k==0) return xstrdup(src?src:""); const char *p=src; while(*p){ int enough=strlen(p)>=k; if(enough&&(p==src||!(isalnum((unsigned char)p[-1])||p[-1]=='_'))&&strncmp(p,key,k)==0&&!(isalnum((unsigned char)p[k])||p[k]=='_')){ append(&out,&len,&cap,val); p+=k; } else { char tmp[2]={*p++,0}; append(&out,&len,&cap,tmp);} } return out?out:xstrdup(""); }
static char *replace_object_like(const char *src,const char *key,const char *val){ char *out=NULL; size_t len=0,cap=0; size_t k=strlen(key); if(k==0) return xstrdup(src?src:""); const char *p=src; while(*p){ int enough=strlen(p)>=k; int before_ok=(p==src||!(isalnum((unsigned char)p[-1])||p[-1]=='_')); int after_ident=(enough&&(isalnum((unsigned char)p[k])||p[k]=='_')); int call_like=(enough&&p[k]=='('); if(enough&&before_ok&&strncmp(p,key,k)==0&&!after_ident&&!call_like){ append(&out,&len,&cap,val); p+=k; } else { char tmp[2]={*p++,0}; append(&out,&len,&cap,tmp);} } return out?out:xstrdup(""); }

static char **split_args(char *s,size_t *argc){ char **out=NULL; *argc=0; char *p=s; while(p&&*p){ while(isspace((unsigned char)*p)||*p==',')p++; if(!*p)break; char *start=p; int quoted=0; int depth=0; while(*p && (quoted || depth>0 || *p!=',')){ if(*p=='"') quoted=!quoted; else if(!quoted && *p=='(') depth++; else if(!quoted && *p==')' && depth>0) depth--; p++; } char saved=*p; *p=0; out=(char**)realloc(out,(*argc+1)*sizeof(char*)); out[(*argc)++]=xstrdup(trim(start)); if(saved) p++; } return out; }

static char *stringize(const char *s){ size_t n=strlen(s); char *o=(char*)malloc(n*2+3); size_t j=0; o[j++]='"'; for(size_t i=0;i<n;i++){ if(s[i]=='"' || s[i]=='\\') o[j++]='\\'; o[j++]=s[i]; } o[j++]='"'; o[j]=0; return o; }

static const char *skip_spaces_const(const char *s){ while(*s && isspace((unsigned char)*s)) s++; return s; }
static Macro *find_invoked_function_macro(Macro *m,const char *head){ for(;m;m=m->next){ if(!m->function_like || !m->name) continue; size_t n=strlen(m->name); if(n && strncmp(head,m->name,n)==0 && *skip_spaces_const(head+n)=='(') return m; } return NULL; }
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


static int emit_expanded_line(char *lineout, Macro **macros, ModernBlock *modern_blocks, int *modern_block_top, int *modern_label_id, char **out, size_t *olen, size_t *ocap, char *err, size_t err_size, int depth){
    if(depth > 64){ snprintf(err, err_size, "macro expansion recursion too deep"); return 0; }
    char *copy=xstrdup(lineout ? lineout : "");
    if(!copy){ snprintf(err, err_size, "out of memory"); return 0; }
    char *save=NULL;
    char *line=strtok_r(copy,"\n",&save);
    int ok=1;
    if(!line){ ok=modern_translate_line(copy, modern_blocks, modern_block_top, modern_label_id, out, olen, ocap, err, err_size); free(copy); return ok; }
    while(line && ok){
        char *current=xstrdup(trim(line));
        if(!current){ snprintf(err, err_size, "out of memory"); ok=0; break; }
        const char *head=current;
        while(*head && isspace((unsigned char)*head)) head++;
        Macro *m=find_invoked_function_macro(*macros,head);
        if(m){
            char *expanded=expand_macro_call(m,head);
            free(current);
            ok=emit_expanded_line(expanded, macros, modern_blocks, modern_block_top, modern_label_id, out, olen, ocap, err, err_size, depth+1);
            free(expanded);
        } else {
            for(Macro*d=*macros;d;d=d->next) if(!d->function_like){ char *ne=replace_object_like(current,d->name,d->body?d->body:""); free(current); current=ne; }
            ok=modern_translate_line(current, modern_blocks, modern_block_top, modern_label_id, out, olen, ocap, err, err_size);
            free(current);
        }
        line=strtok_r(NULL,"\n",&save);
    }
    free(copy);
    return ok;
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
    int in_macro=0; int macro_brace=0; Macro *cur=NULL;
    CondFrame cond[64]; int ctop=0; int active=1;
    ModernBlock modern_blocks[64]; int modern_block_top=0;
    static int modern_label_id=0;
    static char *pragma_once_paths[1024];
    static size_t pragma_once_count=0;
    int line_no=1;
    int logical_line_no=1;
    char logical_file[1024]; snprintf(logical_file,sizeof(logical_file),"%s",path);
    for(size_t i=0;i<pragma_once_count;i++){ if(path_equals(pragma_once_paths[i], path)){ free(dir); free(copy); return 1; } }
    while(line){
        char *raw=line;
        int in_quote=0;
        for(char *scan=raw; *scan; scan++){
            if(*scan=='"' && (scan==raw || scan[-1]!='\\')) in_quote=!in_quote;
            if(!in_quote && scan[0]=='/' && scan[1]=='/'){ *scan=0; break; }
        }
        char *t=trim(raw);
        char modern_inc_name[1024]={0};
        if(modern_include_name(t, modern_inc_name, sizeof(modern_inc_name))){
            if(active){
                char inc[1024];
                resolve_include_path(inc,sizeof(inc),dir,modern_inc_name);
                char *it=read_file(inc,err,err_size); if(!it)goto fail;
                char *processed=NULL; if(!preprocess_text(inc,it,macros,&processed,err,err_size)){free(it);goto fail;}
                append(out,&olen,&ocap,processed?processed:""); free(processed); free(it);
            }
        }
        else if(starts(t,"#include")){
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
                resolve_include_path(inc,sizeof(inc),dir,inc_name);
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
                    m->function_like=1;
                    m->body=xstrdup(trim(rp+1));
                } else {
                    m->function_like=0;
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

        else if(starts(t,"#for")){
            if(active){
                char *p=trim(t+4);
                char *in_kw=strstr(p," in ");
                if(!in_kw){ snprintf(err,err_size,"bad #for in %s:%d",path,line_no); goto fail; }
                *in_kw=0;
                char *ident=trim(p);
                char *items=trim(in_kw+4);
                char *colon=strrchr(items,':');
                if(colon) *colon=0;
                char *body=NULL; size_t blen=0,bcap=0;
                int nested=0;
                line=strtok_r(NULL,"\n",&save); line_no++; logical_line_no++;
                while(line){
                    char *bt=trim(line);
                    if(starts(bt,"#for")) nested++;
                    if(starts(bt,"#endfor")){ if(nested==0) break; nested--; }
                    append(&body,&blen,&bcap,line); append(&body,&blen,&bcap,"\n");
                    line=strtok_r(NULL,"\n",&save); line_no++; logical_line_no++;
                }
                if(!line){ free(body); snprintf(err,err_size,"unterminated #for in %s",path); goto fail; }
                char *itemcopy=xstrdup(items); size_t ac=0; char **vals=split_args(itemcopy,&ac);
                for(size_t i=0;i<ac;i++){
                    macro_push_object(macros, ident, trim(vals[i]));
                    char *processed=NULL;
                    if(!preprocess_text(path, body?body:"", macros, &processed, err, err_size)){
                        macro_remove(macros, ident);
                        for(size_t j=0;j<ac;j++) free(vals[j]);
                        free(vals); free(itemcopy); free(body);
                        goto fail;
                    }
                    append(out,&olen,&ocap,processed?processed:"");
                    free(processed);
                    macro_remove(macros, ident);
                    free(vals[i]);
                }
                free(vals); free(itemcopy); free(body);
            }
        }
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
        else if(starts(t,"macro ")){
            if(active){
                char *p=trim(t+6);
                char *delim=strrchr(p,'{');
                char *colon_delim=strrchr(p,':');
                if(!delim || (colon_delim && colon_delim>delim)) delim=colon_delim;
                char *rp=NULL;
                if(delim){ for(char *q=delim; q>p; q--){ if(q[-1]==')'){ rp=q-1; break; } } }
                char *lp=NULL;
                if(rp){ for(char *q=rp; q>p; q--){ if(q[-1]=='('){ lp=q-1; break; } } }
                char *after=rp?trim(rp+1):NULL;
                int brace=after && *after=='{';
                int colon=after && *after==':';
                if(!lp||!rp||(!colon&&!brace)){snprintf(err,err_size,"bad macro header in %s:%d: %s",path,line_no,t);goto fail;}
                cur=(Macro*)calloc(1,sizeof(*cur));
                cur->name=strndup2(p,(size_t)(lp-p));
                char *argtext=strndup2(lp+1,(size_t)(rp-lp-1));
                cur->args=split_args(argtext,&cur->argc);
                cur->function_like=1;
                free(argtext);
                in_macro=1;
                macro_brace=brace;
            }
        }
        else if(in_macro && ((!macro_brace && strcmp(t,"endmacro")==0) || (macro_brace && strcmp(t,"}")==0))){ cur->next=*macros; *macros=cur; cur=NULL; in_macro=0; macro_brace=0; }
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

            if(!emit_expanded_line(lineout, macros, modern_blocks, &modern_block_top, &modern_label_id, out, &olen, &ocap, err, err_size, 0)){ free(lineout); goto fail; } free(lineout);
        }
        line=strtok_r(NULL,"\n",&save); line_no++; logical_line_no++;
    }
    if(modern_block_top!=0){ snprintf(err,err_size,"unterminated modern syntax block in %s",path); goto fail; }
    if(ctop!=0){ snprintf(err,err_size,"unterminated conditional block in %s",path); goto fail; }
    free(dir); free(copy); return 1;
fail:
    free(dir); free(copy); return 0;
}

int scml_preprocess_file(const char *path, char **out_text, char *err, size_t err_size){ Macro *macros=NULL; seed_env_defines(&macros); char *txt=read_file(path,err,err_size); if(!txt){ macro_free(macros); return 0; } *out_text=NULL; int ok=preprocess_text(path,txt,&macros,out_text,err,err_size); free(txt); macro_free(macros); return ok; }

static int add_stmt(ScmlProgram*p,ScmlStatement*s){ if(p->count==p->capacity){size_t nc=p->capacity?p->capacity*2:32; ScmlStatement*ni=(ScmlStatement*)realloc(p->items,nc*sizeof(*ni)); if(!ni)return 0; p->items=ni;p->capacity=nc;} p->items[p->count++]=*s; return 1; }
static int parse_int(const char *s) {
    const char *p = s;
    int sign = 1;
    int has_hex_alpha = 0;
    if (*p == '+' || *p == '-') {
        if (*p == '-') sign = -1;
        p++;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) return (int)strtol(s, NULL, 16);
    for (const char *q = p; *q; q++) {
        if ((*q >= 'a' && *q <= 'f') || (*q >= 'A' && *q <= 'F')) { has_hex_alpha = 1; break; }
    }
    if (has_hex_alpha) return sign * (int)strtol(p, NULL, 16);
    return (int)strtol(s, NULL, 10);
}
static int is_float_token(const char*s){ if(*s=='-'||*s=='+')s++; int dot=0,digit=0; while(*s){ if(*s=='.'){ if(dot)return 0; dot=1; } else if(isdigit((unsigned char)*s)) digit=1; else return 0; s++; } return dot&&digit; }
static int is_number_token(const char*s){ if(*s=='-'||*s=='+')s++; if(!*s)return 0; if(s[0]=='0' && (s[1]=='x'||s[1]=='X')) s+=2; if(!*s)return 0; while(*s){ if(!isxdigit((unsigned char)*s))return 0; s++; } return 1; }

int scml_parse_file(const char *path, ScmlProgram *program, char *err, size_t err_size){ memset(program,0,sizeof(*program)); char *txt=NULL; if(!scml_preprocess_file(path,&txt,err,err_size))return 0; char *save=NULL,*line=strtok_r(txt,"\n",&save); int ln=1; while(line){ ScmlTokenList toks; if(!scml_lex_line(line,ln,&toks,err,err_size)){free(txt);return 0;} if(toks.count){ ScmlStatement st; memset(&st,0,sizeof(st)); st.line=ln; size_t idx=0; if(toks.items[0].type==SCML_TOK_COLON && toks.count>1){ st.label=xstrdup(toks.items[1].text); if(!add_stmt(program,&st)){free(txt);return 0;} idx=2; }
            if(idx<toks.count){ memset(&st,0,sizeof(st)); st.line=ln; const ScmlOpcodeInfo *info=NULL; char *op=toks.items[idx].text; const char *assign_opcode=NULL; int assign = (idx + 2 < toks.count && strcmp(toks.items[idx + 1].text, "=") == 0); if(idx + 2 < toks.count) assign_opcode=compound_assignment_opcode(toks.items[idx + 1].text); if(assign||assign_opcode){ info=scml_opcode_from_name(assign?"SET":assign_opcode); if(!info){snprintf(err,err_size,"line %d: unsupported assignment operator '%s'",ln,toks.items[idx + 1].text);free(txt);return 0;} st.opcode=info->opcode; ScmlOperand *dst=&st.operands[st.operand_count++]; dst->type=SCML_OPERAND_VAR; dst->text=xstrdup(toks.items[idx].text); if(!assign){ ScmlOperand *src=&st.operands[st.operand_count++]; src->type=SCML_OPERAND_VAR; src->text=xstrdup(toks.items[idx].text); } ScmlToken *tk=&toks.items[idx+2]; ScmlOperand *val=&st.operands[st.operand_count++]; val->text=xstrdup(tk->text); if(tk->type==SCML_TOK_STRING)val->type=SCML_OPERAND_STRING; else if(tk->type==SCML_TOK_LABEL_REF)val->type=SCML_OPERAND_ADDRESS; else if(tk->type==SCML_TOK_NUMBER && is_float_token(tk->text)){val->type=SCML_OPERAND_FLOAT;val->real=(float)strtod(tk->text,NULL);} else if(tk->type==SCML_TOK_NUMBER){val->type=SCML_OPERAND_INT;val->integer=parse_int(tk->text);} else val->type=SCML_OPERAND_VAR; idx += 3; }
            else { size_t oplen=strlen(op); if(oplen&&op[oplen-1]==':')op[--oplen]=0; if(is_number_token(op)) info=scml_opcode_from_scm_code((uint16_t)strtol(op,NULL,16)); else info=scml_opcode_from_name(op); if(!info && strchr(op,'.')){ info=scml_opcode_from_name("CALL_NATIVE"); } if(!info){snprintf(err,err_size,"line %d: unknown opcode '%s'",ln,op);free(txt);return 0;} st.opcode=info->opcode; if(idx+1<toks.count&&toks.items[idx+1].type==SCML_TOK_COLON)idx++; idx++; if(info->opcode==SCML_OP_CALL_NATIVE && strchr(op,'.')){ ScmlOperand *callee=&st.operands[st.operand_count++]; callee->type=SCML_OPERAND_STRING; callee->text=xstrdup(op); if(idx<toks.count && toks.items[idx].type==SCML_TOK_COLON) idx++; } for(;idx<toks.count && st.operand_count<SCML_OPERANDS_MAX;idx++){ ScmlToken *tk=&toks.items[idx]; ScmlOperand *o=&st.operands[st.operand_count++]; o->text=xstrdup(tk->text); if(tk->type==SCML_TOK_STRING)o->type=SCML_OPERAND_STRING; else if(tk->type==SCML_TOK_LABEL_REF)o->type=SCML_OPERAND_ADDRESS; else if(tk->type==SCML_TOK_NUMBER && is_float_token(tk->text)){o->type=SCML_OPERAND_FLOAT;o->real=(float)strtod(tk->text,NULL);} else if(tk->type==SCML_TOK_NUMBER){o->type=SCML_OPERAND_INT;o->integer=parse_int(tk->text);} else o->type=SCML_OPERAND_VAR; } }
            if(idx<toks.count){snprintf(err,err_size,"line %d: too many operands for %s",ln,info->name);free(txt);return 0;} if(st.operand_count<info->min_args||st.operand_count>info->max_args){snprintf(err,err_size,"line %d: %s expects %u..%u args",ln,info->name,info->min_args,info->max_args);free(txt);return 0;} if(!add_stmt(program,&st)){free(txt);return 0;} } }
        scml_token_list_free(&toks); line=strtok_r(NULL,"\n",&save); ln++; }
    free(txt); return 1; }

void scml_program_free(ScmlProgram *p){ for(size_t i=0;i<p->count;i++){ free(p->items[i].label); for(size_t j=0;j<p->items[i].operand_count;j++)free(p->items[i].operands[j].text);} free(p->items); memset(p,0,sizeof(*p)); }
