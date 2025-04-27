#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
using namespace std;

//--------------------------------------------------------------
// Token 类型定义（枚举）  
enum TokenType {
    TK_KEYWORD,
    TK_IDENTIFIER,
    TK_CONSTANT,
    TK_DELIMITER,
    TK_OPERATOR,
    TK_UNKNOWN  // 出错时可以赋值为未知类型
};

// 辅助函数：将 TokenType 转换为字符串
string tokenTypeToString(TokenType type) {
    switch (type) {
    case TK_KEYWORD:     return "KEYWORD";
    case TK_IDENTIFIER:  return "IDENTIFIER";
    case TK_CONSTANT:    return "CONSTANT";
    case TK_DELIMITER:   return "DELIMITER";
    case TK_OPERATOR:    return "OPERATOR";
    default:             return "UNKNOWN";
    }
}

// 辅助函数：将文法中左侧字符串转换为 TokenType 枚举
TokenType getTokenTypeFromString(const string& s) {
    if (s == "KEYWORD")     return TK_KEYWORD;
    if (s == "IDENTIFIER")  return TK_IDENTIFIER;
    if (s == "CONSTANT")    return TK_CONSTANT;
    if (s == "DELIMITER")   return TK_DELIMITER;
    if (s == "OPERATOR")    return TK_OPERATOR;
    return TK_UNKNOWN;
}

//--------------------------------------------------------------
// Token 结构（三元组：行号、token 类型、内容）
struct Token {
    int line;
    TokenType type;
    string lexeme;
};

//--------------------------------------------------------------
// GrammarToDFA 模块：
// 根据正则文法生成 NFA，再确定化生成 DFA，最后对 DFA 进行最小化。
class GrammarToDFA {
public:
    // NFA 状态结构；tokenType=-1表示非终结（非接受状态），否则表示该状态为接受状态，存储 token 类型
    struct NFAState {
        int id;
        int tokenType; // 当 >=0 表示该状态可接受并记录 token 类型
        // 转移表：由输入字符到达状态集合（NFA允许多重转移）
        map<char, set<int>> transitions;
        NFAState(int id_) : id(id_), tokenType(-1) {}
    };

    // DFA 状态结构
    struct DFAState {
        int id;
        int tokenType; // 如果该状态为接受状态，则 tokenType >=0，否则为 -1
        // DFA 转移表：确定性转移，字符 -> DFA 状态 id
        map<char, int> transitions;
        DFAState() : id(0), tokenType(-1) {}
    };

    // 构造函数：初始化内部数据
    GrammarToDFA() { }

    // 根据 grammarFile 构造 NFA、转换为 DFA 并最小化，最终生成 dfaStates 与起始状态 dfaStartState
    void buildDFAFromGrammar(const string& grammarFile) {
        // 1. 构造 NFA（实际上采用类似字典树构造方式）
        buildNFA(grammarFile);
        // 2. 子集构造法生成 DFA
        subsetConstructDFA();
        // 3. 最小化 DFA
        minimizeDFA();
        // 调试输出 DFA 转换表
        printDFA();
    }

    // 获取生成的最小化后的 DFA 状态表
    vector<DFAState> getDFA() const { return dfaStates; }
    // 获取 DFA 起始状态 id
    int getDFAStartState() const { return dfaStartState; }

private:
    // 存储 NFA 状态
    vector<NFAState> nfaStates;
    // DFA 状态表
    vector<DFAState> dfaStates;
    int dfaStartState;

    // 辅助：创建新的 NFA 状态，返回新状态 id
    int newNFAState() {
        int id = nfaStates.size();
        nfaStates.push_back(NFAState(id));
        return id;
    }

    // 辅助：去除字符串首尾空白字符
    string trim(const string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }

    // 使用正则文法构造 NFA：
    // 采用“公共起始状态+每个模式的字面串插入”的方式。
    // grammar.txt 中每一行格式：TOKEN_TYPE -> pattern
    // 构造时先创建一个公共起始状态，然后对每个产生式：
    //    从公共起始状态出发，顺序读取 pattern 的每个字符：
    //       若已有相应转移则沿用，否则创建新状态并建立转移
    //    最后标记该状态为接受状态并记录对应 token 类型
    void buildNFA(const string& grammarFile) {
        ifstream fin(grammarFile);
        if (!fin)
            throw runtime_error("无法打开语法文件：" + grammarFile);

        // 创建公共起始状态
        int startState = newNFAState();

        string line;
        while (getline(fin, line)) {
            if (line.empty())
                continue;
            // 分割 "->"
            size_t pos = line.find("->");
            if (pos == string::npos)
                continue;
            string lhs = trim(line.substr(0, pos));
            string pattern = trim(line.substr(pos + 2));
            if (lhs.empty() || pattern.empty())
                continue;
            TokenType tkType = getTokenTypeFromString(lhs);
            // 插入 pattern 到 NFA
            int current = startState;
            for (char c : pattern) {
                // 若当前状态已有 c 字符的转移，则沿用；否则新建
                if (nfaStates[current].transitions.count(c)) {
                    // 取第一个状态（本 NFA 构造方式下转移集合仅有一个元素）
                    int next = *(nfaStates[current].transitions[c].begin());
                    current = next;
                }
                else {
                    int next = newNFAState();
                    nfaStates[current].transitions[c].insert(next);
                    current = next;
                }
            }
            // 到达 pattern 末尾，标记为接受状态，如果已有 tokenType，则根据优先级（这里简单取较小的值）做调整
            if (nfaStates[current].tokenType == -1)
                nfaStates[current].tokenType = tkType;
            else
                nfaStates[current].tokenType = min(nfaStates[current].tokenType, (int)tkType);
        }
        fin.close();
    }

    // 子集构造法：将 NFA 转为 DFA
    void subsetConstructDFA() {
        // 因为我们的 NFA 构造时无 epsilon 转移，所以初始状态集合即公共起始状态
        set<int> startSet = { 0 };  // 公共起始状态 id 为 0

        // 映射：NFA 状态集合 -> DFA 状态 id
        map<set<int>, int> dfaStateMapping;
        queue<set<int>> workQueue;

        dfaStateMapping[startSet] = 0;
        workQueue.push(startSet);

        // 初始化 DFA 状态 0
        DFAState startDFA;
        startDFA.id = 0;
        // 如果集合中任一 NFA 状态为接受状态，则 DFA 状态为接受状态，并设置 tokenType（优先采用数值较小的）
        for (int stateId : startSet) {
            if (nfaStates[stateId].tokenType != -1) {
                if (startDFA.tokenType == -1)
                    startDFA.tokenType = nfaStates[stateId].tokenType;
                else
                    startDFA.tokenType = min(startDFA.tokenType, nfaStates[stateId].tokenType);
            }
        }
        dfaStates.push_back(startDFA);
        dfaStartState = 0;
        int nextDfaId = 1;

        // 收集所有输入符号
        set<char> symbols;
        for (auto& state : nfaStates)
            for (auto& p : state.transitions)
                symbols.insert(p.first);

        while (!workQueue.empty()) {
            set<int> currentSet = workQueue.front();
            workQueue.pop();
            int currentDfaId = dfaStateMapping[currentSet];

            // 对于每个输入符号，计算下一个 NFA 状态集合
            for (char sym : symbols) {
                set<int> nextSet;
                for (int nfaId : currentSet) {
                    if (nfaStates[nfaId].transitions.count(sym))
                        nextSet.insert(nfaStates[nfaId].transitions[sym].begin(), nfaStates[nfaId].transitions[sym].end());
                }
                if (nextSet.empty())
                    continue;
                if (dfaStateMapping.find(nextSet) == dfaStateMapping.end()) {
                    dfaStateMapping[nextSet] = nextDfaId;
                    DFAState newDfa;
                    newDfa.id = nextDfaId;
                    // 如果集合中有接受状态，确定 tokenType
                    for (int s : nextSet) {
                        if (nfaStates[s].tokenType != -1) {
                            if (newDfa.tokenType == -1)
                                newDfa.tokenType = nfaStates[s].tokenType;
                            else
                                newDfa.tokenType = min(newDfa.tokenType, nfaStates[s].tokenType);
                        }
                    }
                    dfaStates.push_back(newDfa);
                    workQueue.push(nextSet);
                    nextDfaId++;
                }
                // 添加转移关系
                int targetDfaId = dfaStateMapping[nextSet];
                dfaStates[currentDfaId].transitions[sym] = targetDfaId;
            }
        }
    }

    // 使用简单划分法对 DFA 进行最小化
    void minimizeDFA() {
        int dfaN = dfaStates.size();
        if (dfaN == 0)
            return;
        // 初始划分：根据是否为接受状态和 tokenType 进行划分
        map<int, set<int>> groups;  // key 为 tokenType（-1 表示非接受状态），value 为该组状态集合
        for (int i = 0; i < dfaN; i++) {
            int key = dfaStates[i].tokenType;
            groups[key].insert(i);
        }
        vector<set<int>> partitions;
        for (auto& p : groups)
            partitions.push_back(p.second);

        // 收集所有输入符号
        set<char> symbols;
        for (auto& st : dfaStates) {
            for (auto& tran : st.transitions)
                symbols.insert(tran.first);
        }

        bool changed = true;
        while (changed) {
            changed = false;
            vector<set<int>> newPartitions;
            for (auto& group : partitions) {
                // 根据每个符号下的转移分成更小的组
                map<string, set<int>> splitter;
                for (int stateId : group) {
                    string key;
                    for (char sym : symbols) {
                        int target = -1;
                        if (dfaStates[stateId].transitions.count(sym))
                            target = dfaStates[stateId].transitions[sym];
                        // 寻找 target 在 partitions 中所属分区的下标
                        int partIndex = findPartition(target, partitions);
                        //通过+","实现唯一状态标签
                        key += to_string(partIndex) + ",";
                    }
                    splitter[key].insert(stateId);
                }
                if (splitter.size() > 1)
                    changed = true;
                for (auto& p : splitter)
                    newPartitions.push_back(p.second);
            }
            partitions = newPartitions;
        }

        // 构造新 DFA：旧状态映射到新状态（各组取一个代表）
        map<int, int> stateMapping;
        int newId = 0;
        for (auto& group : partitions) {
            for (int stateId : group)
                stateMapping[stateId] = newId;
            newId++;
        }
        vector<DFAState> newDfaStates(newId);
        for (auto& group : partitions) {
            int rep = *group.begin();
            int newStateId = stateMapping[rep];
            newDfaStates[newStateId].id = newStateId;
            newDfaStates[newStateId].tokenType = dfaStates[rep].tokenType;
        }
        for (int i = 0; i < dfaN; i++) {
            int newSrc = stateMapping[i];
            for (auto& tran : dfaStates[i].transitions) {
                int newDst = stateMapping[tran.second];
                newDfaStates[newSrc].transitions[tran.first] = newDst;
            }
        }
        dfaStates = newDfaStates;
        dfaStartState = stateMapping[dfaStartState];
    }

    // 辅助：给定状态 target 在 partitions 中所属的分区下标，如果 target 为 -1 则返回 -1
    int findPartition(int target, const vector<set<int>>& partitions) {
        if (target == -1)
            return -1;
        for (int i = 0; i < partitions.size(); i++) {
            if (partitions[i].count(target))
                return i;
        }
        return -1;
    }

    // 输出 DFA 转换表，便于调试
    void printDFA() {
        cout << "===== 最小化后的 DFA 转换表 =====" << endl;
        for (auto& state : dfaStates) {
            cout << "状态 " << state.id;
            if (state.tokenType != -1)
                cout << " [接受, " << tokenTypeToString((TokenType)state.tokenType) << "]";
            cout << " :" << endl;
            for (auto& tran : state.transitions) {
                cout << "   " << tran.first << " -> " << tran.second << endl;
            }
        }
        cout << "起始状态: " << dfaStartState << endl;
        cout << "===================================" << endl;
    }
};

//--------------------------------------------------------------
// DfaLexicalAnalyzer：使用由正则文法构造的 DFA 对用户输入的源代码进行词法扫描
class DfaLexicalAnalyzer {
public:
    // 构造时传入 DFA 状态表及起始状态
    DfaLexicalAnalyzer(const vector<GrammarToDFA::DFAState>& dfaStates_, int dfaStartState_)
        : dfaStates(dfaStates_), dfaStartState(dfaStartState_) { }

    // 分析源代码文件，返回 token 列表；使用最长匹配规则
    vector<Token> analyze(const string& sourceFile) {
        vector<Token> tokens;
        ifstream fin(sourceFile);
        if (!fin)
            throw runtime_error("无法打开源代码文件：" + sourceFile);

        string line;
        int lineNo = 0;
        while (getline(fin, line)) {
            lineNo++;
            int pos = 0;
            while (pos < line.size()) {
                // 若当前字符为空白则跳过
                if (isspace(line[pos])) { pos++; continue; }
                int currentState = dfaStartState;
                int lastAcceptPos = -1;
                int lastAcceptToken = -1;
                int j = pos;
                // 从当前位置开始尽可能匹配最长 token
                while (j < line.size() && dfaStates[currentState].transitions.count(line[j])) {
                    currentState = dfaStates[currentState].transitions.at(line[j]);
                    if (dfaStates[currentState].tokenType != -1) {
                        lastAcceptPos = j;
                        lastAcceptToken = dfaStates[currentState].tokenType;
                    }
                    j++;
                }
                if (lastAcceptPos != -1) {
                    // 识别出 token：取从 pos 到 lastAcceptPos 的子串
                    Token tk;
                    tk.line = lineNo;
                    tk.type = (TokenType)lastAcceptToken;
                    tk.lexeme = line.substr(pos, lastAcceptPos - pos + 1);
                    tokens.push_back(tk);
                    pos = lastAcceptPos + 1;
                }
                else {
                    // 没有匹配成功，报错或跳过一个字符
                    cerr << "词法错误：第 " << lineNo << " 行，第 " << pos + 1 << " 个字符无法识别" << endl;
                    pos++;
                }
            }
        }
        fin.close();
        return tokens;
    }
private:
    vector<GrammarToDFA::DFAState> dfaStates;
    int dfaStartState;
};

//--------------------------------------------------------------
// 主函数：
// 1. 根据 grammar.txt 构造 DFA
// 2. 用该 DFA 对 source.txt 中的源代码进行词法扫描，输出 token 列表
int main(int argc, char* argv[]) {
    try {
        // 检查命令行参数，要求提供两个文件名
        if (argc < 3) {
            cerr << "用法: " << argv[0] << " <grammar文件> <source文件>" << endl;
            return 1;
        }
        string grammarFile = argv[1];
        string sourceFile = argv[2];

        // 1. 从正则文法构造 DFA（不再使用 Trie）
        GrammarToDFA g2dfa;
        g2dfa.buildDFAFromGrammar(grammarFile);

        // 2. 获取生成的 DFA 状态表及起始状态
        vector<GrammarToDFA::DFAState> dfaStates = g2dfa.getDFA();
        int dfaStartState = g2dfa.getDFAStartState();

        // 3. 使用生成的 DFA 扫描源代码，生成 token 列表
        DfaLexicalAnalyzer lexer(dfaStates, dfaStartState);
        vector<Token> tokens = lexer.analyze(sourceFile);

        // 4. 输出 token 列表
        cout << "\n===== Token 列表 =====" << endl;
        cout << "行号\t类别\t\tToken 内容" << endl;
        for (auto& tk : tokens) {
            cout << tk.line << "\t" << tokenTypeToString(tk.type) << "\t\t" << tk.lexeme << endl;
        }
    }
    catch (const exception& e) {
        cerr << "错误: " << e.what() << endl;
        return 1;
    }
    return 0;
}
