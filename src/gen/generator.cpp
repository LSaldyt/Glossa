#include "generator.hpp"
#include "../syntax/symbols/export.hpp"

namespace gen 
{

Generator::Generator(vector<string> filenames, string directory)
{
    readStructureFile(directory + "file");
    for (auto filename : filenames)
    {
        print("Reading constructor file: " + filename);
        construction_map[filename] = readConstructor(directory + filename);
    }
}

void Generator::readStructureFile(string filename)
{
    auto content = readFile(filename);
    string last_type = "none";
    string last_extension = "none";
    vector<string> default_content;

    for (auto line : content)
    {
        auto terms = lex::seperate(line, {make_tuple(" ", false)});
        if (not terms.empty())
        {
            if (terms[0] == "file")
            {
                if (last_type != "none")
                {
                    file_constructors.push_back(make_tuple(last_type, FileConstructor(last_extension, default_content)));
                    default_content.clear();
                }
                assert(terms.size() == 3);
                last_type      = terms[1];
                last_extension = terms[2];
            }
            else
            {
                default_content.push_back(line);
            }
        }
    }
    if (last_type != "none")
    {
        file_constructors.push_back(make_tuple(last_type, FileConstructor(last_extension, default_content)));
        default_content.clear();
    }
}

vector<tuple<string, Constructor>> Generator::readConstructor(string filename)
{
    print("Reading Constructor: " + filename);
    auto content = readFile(filename);
    
    // Seperate constructor into header and source constructors
    assert(contains(content, "name"s));
    assert(contains(content, "defines"s));
    auto name_i    = std::find(content.begin(), content.end(), "name");
    auto defines_i = std::find(content.begin(), content.end(), "defines");
    auto declarations = vector<string>(content.begin(), name_i);
    auto name         = vector<string>(name_i    + 1, defines_i);

    auto symbol_storage_generator = generateSymbolStorageGenerator(declarations);
    vector<string> definitions;

    string type = "definitions";
    string tag;
    bool first = true;
    auto last_it = content.begin();
    auto it = content.begin();

    // Iterate over file types for each constructor file
    vector<tuple<string, Constructor>> constructors;
    for (auto file_constructor : file_constructors)
    {
        tag = get<0>(file_constructor);
        assert(contains(content, tag));
        it = std::find(content.begin(), content.end(), tag);

        if (first)
        {
            definitions  = vector<string>(defines_i + 1, it);
            last_it = it;
            first = false;
        }
        else
        {
            auto body = vector<string>(last_it + 1, it);
            auto constructor = Constructor(symbol_storage_generator, generateBranch(body, symbol_storage_generator), definitions, name);
            constructors.push_back(make_tuple(type, constructor));
            last_it = it;
        }
        type = tag;
    }
    auto body = vector<string>(last_it + 1, content.end());
    auto constructor = Constructor(symbol_storage_generator, generateBranch(body, symbol_storage_generator), definitions, name);
    constructors.push_back(make_tuple(type, constructor));

    return constructors;
}

Branch Generator::generateBranch(vector<string> content, SymbolStorageGenerator symbol_storage_generator)
{
    vector<LineConstructor> line_constructors;
    vector<Branch>          nested_branches;
    
    auto default_body    = content.begin();
    auto if_body_start   = content.begin();
    auto else_body_start = content.begin(); // For collecting and processing contents of an if statement body
    auto if_body_end     = content.begin();
    auto else_body_end   = content.begin();

    bool in_conditional = false;
    bool has_else = false;

    const auto addNestedBranch = [&](auto start, auto end, auto conditionline, bool inverse)
    {
        vector<string> body(start, end);
        auto nested_branch                = generateBranch(body, symbol_storage_generator);
        auto original_condition_terms     = lex::seperate(*conditionline, {make_tuple(" ", false)});
        if (inverse)
        {
            nested_branch.condition_evaluator = inverseBranch(generateConditionEvaluator(slice(original_condition_terms, 1)));
        }
        else
        {
            nested_branch.condition_evaluator = generateConditionEvaluator(slice(original_condition_terms, 1));
        }
        nested_branches.push_back(nested_branch);
    };

    auto it = content.begin();
    while (it != content.end())
    {
        auto terms = lex::seperate(*it, {make_tuple(" ", false)});
        if (not terms.empty())
        {
            auto keyword = terms[0];
            if (keyword == "branch")
            {
                if (default_body != it)
                {
                    nested_branches.push_back(Branch(defaultBranch, line_constructors, {}));
                    line_constructors.clear();
                    default_body = it;
                }
                if_body_start = it;
                in_conditional = true;
            }
            else if (keyword == "elsebranch")
            {
                if_body_end     = it;
                else_body_start = it;
                has_else = true;
            }
            else if (keyword == "end")
            {
                if (not has_else)
                {
                    if_body_end = it;
                }
                addNestedBranch(if_body_start + 1, if_body_end, if_body_start, false);

                if (has_else)
                {
                    else_body_end = it;
                    addNestedBranch(else_body_start + 1, else_body_end, if_body_start, true);
                }
                in_conditional = false;
                default_body = it;
            }
            else if (not in_conditional)
            {
                line_constructors.push_back(generateLineConstructor(terms));
            }
        }
        it++;
    }
    if (default_body != content.end())
    {
        nested_branches.push_back(Branch(defaultBranch, line_constructors, {}));
        line_constructors.clear();
    }

    return Branch(defaultBranch, line_constructors, nested_branches);
}

LineConstructor Generator::generateLineConstructor(vector<string> terms)
{
    return [terms, this](unordered_set<string>& names, SymbolStorage& storage, string filetype){
        string representation = "";
        if (not terms.empty())
        {
            auto keyword = terms[0];
            if (keyword == "sep")
            {
                assert(terms.size() == 3 or terms.size() == 4 or terms.size() == 5);
                assert(contains(get<1>(storage), terms[2]));
                auto symbols = get<1>(storage)[terms[2]];
                string formatter = "@";
                if (terms.size() > 3)
                {
                    formatter = terms[3];
                }
                representation += sepWith(*this, symbols, names, filetype, terms[1], formatter);
            }
            else if (keyword == "format")
            {
                assert(terms.size() == 3);
                assert(contains(get<0>(storage), terms[1]));
                auto symbol    = get<0>(storage)[terms[1]];
                auto formatter = terms[2];
                representation += format(symbol->representation(*this, names, filetype), formatter) + " ";
            }
            else
            {
                for (auto t : terms)
                {
                    if (t[0] == '$')
                    {
                        auto symbol = get<0>(storage)[t];
                        representation += symbol->representation(*this, names, filetype) + " ";
                    }
                    else
                    {
                        representation += t + " ";
                    }
                }
            }
        }
        replaceAll(representation, "&", " ");
        replaceAll(representation, "^", "\n");
        return representation;
    };
}

SymbolStorageGenerator Generator::generateSymbolStorageGenerator(vector<string> content)
{
    return [content](vector<vector<shared_ptr<Symbol>>>& symbol_groups){
        SymbolStorage storage;
        for (auto line : content)
        {
            auto terms = lex::seperate(line, {make_tuple(" ", false)});
            assert(terms.size() == 3 or 
                   terms.size() == 4);
            auto identifier = terms[0];
            if (terms.size() == 3)
            {
                auto keyword = terms[0];
                if (keyword == "append")
                {
                    assert(contains(get<1>(storage), terms[2]));
                    assert(contains(get<0>(storage), terms[1]));
                    auto& symbols = get<1>(storage)[terms[2]];
                    auto& symbol  = get<0>(storage)[terms[1]];
                    symbols.push_back(symbol);
                }
                else if (keyword == "concat")
                {
                    assert(contains(get<1>(storage), terms[1]));
                    assert(contains(get<1>(storage), terms[2]));
                    auto& a_symbols = get<1>(storage)[terms[1]];
                    auto& b_symbols = get<1>(storage)[terms[2]];
                    concat(a_symbols, b_symbols);
                }
                else
                {
                    int index = std::stoi(terms[2]);
                    get<1>(storage)[identifier] = symbol_groups[index]; 
                }
            }
            else if (terms.size() == 4)
            {
                if (terms[2] == "names")
                {
                    vector<shared_ptr<Symbol>> names;
                    int index = std::stoi(terms[3]);
                    for (auto symbol : symbol_groups[index])
                    {
                        names.push_back(make_shared<Identifier>(Identifier(symbol->name())));
                    }
                    get<1>(storage)[identifier] = names;
                }
                else
                {
                    int index_a = std::stoi(terms[2]);
                    int index_b = std::stoi(terms[3]);
                    get<0>(storage)[identifier] = symbol_groups[index_a][index_b]; 
                }
            }
        }
        return storage;
    };
}

ConditionEvaluator Generator::generateConditionEvaluator(vector<string> terms)
{
    assert(not terms.empty());
    auto keyword = terms[0];
    if (keyword == "defined")
    {
        assert(terms.size() == 2);
        auto identifier = terms[1];
        return [identifier](unordered_set<string>& names, SymbolStorage& symbol_storage, const vector<string>&)
        {
            string to_define = get<0>(symbol_storage)[identifier]->name();
            return contains(names, to_define) or
                   contains(names, to_define); 
        };
    }
    else if (keyword == "equalTo")
    {
        assert(terms.size() == 3);
        return [terms](unordered_set<string>& names, SymbolStorage& symbol_storage, const vector<string>&)
        {
            assert(contains(get<0>(symbol_storage), terms[1]));
            auto name = get<0>(symbol_storage)[terms[1]]->name();
            return name == terms[2]; 
        };
    }
    else if (keyword == "empty")
    {
        assert(terms.size() == 2);
        return [terms](unordered_set<string>& names, SymbolStorage& symbol_storage, const vector<string>&)
        {
            assert(contains(get<1>(symbol_storage), terms[1]));
            return get<1>(symbol_storage)[terms[1]].empty();
        };
    }
    else if (keyword == "nonempty")
    {
        assert(terms.size() == 2);
        return [terms](unordered_set<string>& names, SymbolStorage& symbol_storage, const vector<string>&)
        {
            assert(contains(get<1>(symbol_storage), terms[1]));
            return not get<1>(symbol_storage)[terms[1]].empty();
        };
    }
    else if (keyword == "both")
    {
        assert(contains(terms, "and"s));
        auto split = std::find(terms.begin(), terms.end(), "and");
        vector<string> first(terms.begin() + 1, split);
        vector<string> second(split + 1, terms.end());
        return [this, first, second](unordered_set<string>& names, SymbolStorage& symbol_storage, const vector<string>& generated)
        {
            auto a = generateConditionEvaluator(first);
            auto b = generateConditionEvaluator(second);

            return a(names, symbol_storage, generated) and b(names, symbol_storage, generated);
        };
    }
    else
    {
        throw named_exception("Constructor keyword " + keyword + " is not defined");
    }
}

vector<tuple<string, string, vector<string>>> Generator::operator()(unordered_set<string>&              names, 
                                                                    vector<vector<shared_ptr<Symbol>>>& symbol_groups, 
                                                                    string                              symbol_type, 
                                                                    string                              filename)
{
    vector<tuple<string, string, vector<string>>> files;
    auto constructors = construction_map[symbol_type];
    for (auto t : constructors)
    {
        auto type        = get<0>(t);
        auto constructor = get<1>(t);

        string extension;
        vector<string> default_content;
        for (auto fc : file_constructors)
        {
            if (get<0>(fc) == type)
            {
                if (filename != "none")
                {
                    for (auto line : get<1>(fc).default_content)
                    {
                        default_content.push_back(format(filename, line));
                    }
                }
                extension = get<1>(fc).extension;
                break;
            }
        } 
        auto constructed = constructor(names, symbol_groups, type);
        concat(default_content, constructed);
        files.push_back(make_tuple(type, extension, default_content));
    }
    return files;
}

}