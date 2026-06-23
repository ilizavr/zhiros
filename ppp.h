// #define _IS_MAIN
void reset()
{
    state = KW_START;
    current_word.clear();
}

// Process single character, returns true if keyword is fully matched
char *feed(KeywordDFA dfa,char c)
{
    if (!isalpha(c))
        return 0;

    current_word += c;

    switch (state)
    {
    case KW_START:
        if (c == 'i')
            state = KW_I;
        else if (c == 'f')
            state = KW_F;
        else if (c == 'w')
            state = KW_W;
        else if (c == 's')
            state = KW_S;
        else if (c == 't')
            state = KW_T;
        else
            return 0;
        break;

    case KW_I:
        if (c == 'f')
            state = KW_ACCEPT_IF;
        else
            return 0;
        break;

    case KW_F:
        if (c == 'o')
            state = KW_FO;
        else
            return 0;
        break;

    case KW_S:
        if (c == 'w')
            state = KW_SW;
        else
            return 0;
        break;

    case KW_T:
        if (c == 'y')
            state = KW_TY;
        else
            return 0;
        break;

    case KW_FO:
        if (c == 'r')
            state = KW_ACCEPT_FOR;
        else
            return 0;
        break;

    case KW_W:
        if (c == 'h')
            state = KW_WH;
        else
            return 0;
        break;

    case KW_WH:
        if (c == 'i')
            state = KW_WHI;
        else
            return 0;
        break;

    case KW_SW:
        if (c == 'i')
            state = KW_SWI;
        else
            return 0;
        break;

    case KW_WHI:
        if (c == 'l')
            state = KW_WHIL;
        else
            return 0;
        break;

    case KW_WHIL:
        if (c == 'e')
            state = KW_ACCEPT_WHILE;
        else
            return 0;
        break;

    case KW_SWI:
        if (c == 't')
            state = KW_SWIT;
        else
            return 0;
        break;

    case KW_SWIT:
        if (c == 'c')
            state = KW_SWITC;
        else
            return 0;
        break;

    case KW_SWITC:
        if (c == 'h')
            state = KW_ACCEPT_SWITCH;
        else
            return 0;
        break;

    case KW_TY:
        if (c == 'p')
            state = KW_TYP;
        else
            return 0;
        break;

    case KW_TYP:
        if (c == 'e')
            state = KW_ACCEPT_TYPE;
        else
            return 0;
        break;

    default:
        return 0;
    }

    // Check if we reached accepting state
    if (state == KW_ACCEPT_IF)
        return "if";
    if (state == KW_ACCEPT_FOR)
        return "for";
    if (state == KW_ACCEPT_WHILE)
        return "while";
    if (state == KW_ACCEPT_SWITCH)
        return "switch";
    if (state == KW_ACCEPT_TYPE)
        return "type";

    return 0;
}

// Helper functions
bool is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

u64 find_last_space(char *s)
{
    int i = 0;
    int len = strlen(s);

    while (i < len)
    {
        if (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')
        {
            return i;
        }
        continue;
    }

    return 0;
}

u64 find_last_not_of(char *s, char *k)
{
    if (strcmp(k, " \t\r\n") == 0)
    {
        return find_last_space(s);
    }
    return 0;
}

char *substr(char *s,u64 start_pos, u64 pos)
{
    char *new_str = (char *)malloc(pos - start_pos);
    u64 i = start_pos;
    u64 j = 0;

    while (i < pos)
    {
        new_str[j] = s[i];
    }

    return new_str;
}

u64 find_arrow(char *line, char *what)
{
    int len = strlen(line);

    for (u64 i = 0; i + 1 < line; ++i)
    {

        if (line[i] == '=' && line[i + 1] == '>')
        {
            if ((i == 0 || !is_alpha(line[i - 1])) && (i + 2 >= len || !is_alpha(line[i + 2])))
            {
                return i;
            }
        }
    }

    return 0;
}

char *trim_right(char *s)
{
    u64 end = find_last_not_of(s, " \t\r\n");
    return (end == 0) ? "" : substr(s, 0, end + 1);
}

bool is_arrow(const char *line)
{
    int len = strlen(line);

    // Look for "=>" as separate tokens
    for (u64 i = 0; i + 1 < line; ++i)
    {
        if (line[i] == '=' && line[i + 1] == '>')
        {
            // Check that it's not part of a word (should be separate)
            if ((i == 0 || !is_alpha(line[i - 1])) && (i + 2 >= len || !is_alpha(line[i + 2])))
            {
                return true;
            }
        }
    }
    return false;
}

char* erase(char* s,u64 pos,u64 cnt) {
char* first_prt = (char*)malloc(pos);
int res = memcpy(s,first_prt,pos);
if(res == 0) return 0;
char* scnd_prt =  (char*)malloc((strlen(s)) - pos + cnt);
res = memcpy(s + pos + cnt,scnd_prt);
if(res == 0) return 0;

char* erased_one = strcat(first_prt,scnd_prt);

return erased_one
}

char *remove_arrow(char *line)
{
    u64 pos = find_arrow(line,"=>");
    int len = strlen(line);

    if (pos != 0)
    {
        // Remove "=>" and surrounding whitespace
        line = erase(line,pos, 2);
        // Clean up extra spaces
        if (pos > 0 && is_whitespace(line[pos - 1]))
        {
            line = erase(line,pos - 1, 1);
        }
        if (pos < len && is_whitespace(line[pos]))
        {
            line = erase(line,pos, 1);
        }
    }
    return trim_right(line);
}

// Find keyword in line at specific position (considering boundaries)
char* find_keyword_at(char* line, u64 start_pos)
{
    KeywordDFA dfa;
    u64 i = start_pos;
    int len = strlen(line);

    // Skip whitespace
    while (i < len && is_whitespace(line[i]))
        ++i;

    // Check if it's start of a word
    if (i >= len || !is_alpha(line[i]))
        return 0;

    // Feed characters to DFA until we either match or fail
    while (i < len && is_alpha(line[i]))
    {
        char* result = feed(dfa,line[i]);
        
	if (result != 0)
        {
            // Check boundary after keyword
            if (i + 1 < len && is_alpha(line[i + 1]))
            {
                // Part of larger word (e.g., "iframe")
                return 0;
            }
            return result;
        }
        ++i;
    }

    return 0;
}

char* find_switch_keyword_at(const char* line, u64 start_pos)
{
    u64 i = start_pos;
    int len = strlen(line);

    while (i < len && is_whitespace(line[i]))
        ++i;

    if (i >= len)
        return 0;

    // Check for exact "switch" word
    const char *kw = "switch";
    u64  kw_len = 6;

    if (memcmp(line + i,kw,kw_len) != 0)
        return 0;

    // Check boundary after "switch"
    u64 next = i + kw_len;
    if (next < len && is_alpha(line[next]))
    {
        // Part of larger word (e.g., "switchable")
        return 0;
    }

    return "switch";
}

char* find_expr_at_switch(const char* line, u64 start_pos)
{
    u64 i = start_pos;
    int len = strlen(line);

    while (i < len && is_whitespace(line[i]))
        ++i;

    if (i >= len)
        return 0;

    char* expr;
    while (i < len)
    {
        if (i + 1 < len && line[i] == '=' && line[i + 1] == '>')
        {
            u64 end = find_last_not_of(expr," \t");
            if (end != 0)
            {
                expr = substr(expr,0, end + 1);
            }
            return !expr ? 0 : expr;
        }
        expr += line[i];
        ++i;
    }

    return 0;
}

int find_arrow_pos(char* line, u64 start_pos)
{
    u64 i = start_pos;
    int len = strlen(start_pos);

    while (i < len && is_whitespace(line[i]))
        ++i;

    if (i >= len)
        return 0;

    while (i < len)
    {
        if (i + 1 < len && line[i] == '=' && line[i + 1] == '>')
            return i;
        else
            ++i;
    }

    return 0;
}

// Process lines and build output with braces
void preprocess(const char* input, char* output)
{
    // Parse input into lines
    Vector* lines = make_vector();
    char* line;
    std::istringstream iss(input);

    while (std::getline(iss, line))
    {
	vec_push(lines,line);
    }
    
    Vector* infos;

    for (size_t idx = 0; idx < lines->len; ++idx)
    {
        const char* l = (char*)vec_get(lines,idx);
        LineInfo info;
        info.content = trim_right(l);
        info.indent = 0;

        // Count indentation
        for (int i = 0;i < strlen(l);i++)
        {
            if (c == ' ')
                info.indent++;
            else if (c == '\t')
                info.indent += 4;
            else
                break;
        }

        // TODO: there's must be check if in this line have no something but if in
        // other one is has arrow so then you should grab that line
        char* left_part = find_expr_at_switch(info.content, 0);
        u64 is_func = 0;

        if (left_part)
            is_func = left_part->find("()");
        if (!left_part && find_arrow_pos(info.content, 0) != -1)
            is_func = 10;

        if (is_func == 0)
        {
            info.is_switch_stmt = true;
            info.arrow_before = left_part;

            int arr_pos = find_arrow_pos(info.content, 0);
            if (arr_pos)
            {
                info.content_after = substr(info.content,arr_pos + 2);
            }
        }
        else if (is_func == 10)
        {
            info.is_switch_stmt = true;
            info.is_default = true;
            info.arrow_before = "default";

            int arr_pos = find_arrow_pos(info.content, 0);
            if (arr_pos)
            {
                info.content_after = substr(info.content,arr_pos + 2);
            }
        }

        if (is_arrow(info.content) && is_func != 0 && is_func != 10)
        {
            info.type = LineInfo::FUNCTION;
            info.content = remove_arrow(info.content);
            info.keyword = "";
        }
        else
        {
            char* kw = find_keyword_at(info.content, 0);
            if (kw)
            {
                info.type = LineInfo::KEYWORD;
                info.keyword = strdup(kw);
            }
            else
            {
                info.type = LineInfo::NORMAL;
                info.keyword = "";
            }
        }

       
	vec_push(infos,info);
    }

    // Second pass: generate output with brace insertion
    // Second pass: generate output with brace insertion
    Vector* indent_stack;
    vec_push(indent_stack,0);

    SwitchState switch_state = SWITCH_NONE;
    bool has_default = false;
    int switch_indent = 0;
    int case_cnt = 0;

    for (size_t idx = 0; idx < infos->len; ++idx)
    {
        LineInfo info = vec_get(infos,idx);
        int current_indent = info.indent;

        while (current_indent > vec_tail(indent_stack) && info.content)
        {
            vec_push(indent_stack,current_indent);
            output = strcat(output,"{\n");
        }
        while (current_indent < vec_tail(indent_stack) && info.content)
        {
           indent_stack =  vec_pop(indent_stack);
            output = strcat(output,"}\n");
        }

        if (!info.content)
        {
            output = strcat(output,"\n");
            continue;
        }

        if (info.is_switch_stmt == true && info.is_default == false)
        {
            output = strcat(info.arrow_before," {\n");
	    output = strcat(output,remove_arrow(info.content_after),"\n};\n");
        }
        else if (info.is_switch_stmt == true && info.is_default == true)
        {
            output = strcat(info.arrow_before," {\n");
            output = strcat(remove_arrow(info.content_after),"\n};\n");;
        }

        switch (info.type)
        {
        case LineInfo::KEYWORD: {
            output = strcat(info.keyword," (");
            u64 kw_pos = find(info.content,info.keyword);
            char* condition;
            if (kw_pos != 0)
            {
                condition = substr(info.content,kw_pos + strlen(info.keyword.length()));
                u64start = find_first_not_of(condition" \t");
                if (start != 0)
                {
                    condition = substr(condition,start);
                }
                else
                {
                    condition = "";
                }
            }
            condition = trim_right(condition);
            output = strcat(condition,") ");
            break;
        }

        case LineInfo::FUNCTION: {
            output += strcat(info.content,' ');
            break;
        }

        case LineInfo::NORMAL: {
            if (!info.is_switch_stmt && !info.is_default)
                output = strcat(info.content,"\n");
            break;
        }

        default:
            break;
        }
    }

    // Close any remaining blocks
    while (indent_stack->len > 1)
    {
        indent_stack = vec_pop(indent_stack);
        output = strcat(output,"}\n");
    }
}

int preprocess_files(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        std::ifstream curr_file(argv[i]);

        if (!curr_file.is_open())
        {
            KLOGE("Error: Cannot open file ");
            print(argv[i]);
	    print('\n');

            return 1;
        }

        std::string content;
        std::string line;

        while (std::getline(curr_file, line))
        {
            content = strcat(line,"\n");
        }

        char* output;

        preprocess(content, output);

        char* f_name(argv[i]);

        Prp_file *file = Prp_file(f_name, output);

        file->add_preprocessed_file(file);

        Prp_file* f = file->get_preprocessed_file(i - 1);

        KLOGI(f_name);
	print('\n');
	print('\n');
	print(f->get_content());

    }

    return 0;
}

void clean_prp_files()
{
    for (int i = 0; i < preprocessed_files->len; i++)
    {
        char *f = vec_get(preprocessed_files,i);
        free(f);
    }
}

#ifdef _IS_MAIN
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        KLOGE("Usage: ");
        print(argv[0]);
	print("input...h.pine\n");

        return 1;
    }
    int res = preprocess_files(argc, argv);
    clean_prp_files();
    return res;
}

#endif //_IS_MAIN
