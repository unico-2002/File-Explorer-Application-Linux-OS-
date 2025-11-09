#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <pwd.h>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <array>
#include <optional>

namespace fs = std::filesystem;

static volatile std::sig_atomic_t g_should_exit = 0;

void handle_sigint(int){ g_should_exit = 1; }

std::string human_size(std::uintmax_t bytes){
    const char* units[] = {"B","KB","MB","GB","TB"};
    int i=0; double v = static_cast<double>(bytes);
    while(v>=1024.0 && i<4){ v/=1024.0; ++i; }
    std::ostringstream oss; oss<< std::fixed << std::setprecision((i==0)?0:1) << v << ' ' << units[i];
    return oss.str();
}

std::string perm_string(const fs::perms p){
    auto bit=[&](fs::perms b, char c){ return ( (p & b) != fs::perms::none ) ? c : '-'; };
    std::string s;
    s += bit(fs::perms::owner_read,'r');
    s += bit(fs::perms::owner_write,'w');
    s += bit(fs::perms::owner_exec,'x');
    s += bit(fs::perms::group_read,'r');
    s += bit(fs::perms::group_write,'w');
    s += bit(fs::perms::group_exec,'x');
    s += bit(fs::perms::others_read,'r');
    s += bit(fs::perms::others_write,'w');
    s += bit(fs::perms::others_exec,'x');
    return s;
}

static std::time_t file_time_to_time_t(fs::file_time_type ftime){
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + system_clock::now());
    return system_clock::to_time_t(sctp);
}

std::string time_string(fs::file_time_type tp){
    std::time_t t = file_time_to_time_t(tp);
    std::tm tm{}; localtime_r(&t, &tm);
    char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

std::string owner_string(const fs::path& p){
    struct stat st{}; if(::stat(p.c_str(), &st)==0){
        if(auto* pw = ::getpwuid(st.st_uid)) return pw->pw_name ? pw->pw_name : std::to_string(st.st_uid);
        return std::to_string(st.st_uid);
    }
    return "-";
}

struct Ctx{ fs::path cwd; bool force = false; };

void cmd_pwd(const Ctx& ctx){ std::cout << ctx.cwd << "\n"; }

void cmd_ls(const Ctx& ctx, const std::vector<std::string>& args){
    bool all=false, longfmt=false; fs::path target = ctx.cwd; bool tree=false; int depth=1;
    for(size_t i=0;i<args.size();++i){
        if(args[i]=="-a") { all=true; }
        else if(args[i]=="-l") { longfmt=true; }
        else if(args[i]=="--tree") { tree=true; depth=10; }
        else if(args[i].rfind("--depth=",0)==0) { tree=true; depth=std::stoi(args[i].substr(9)); }
        else { target = fs::path(args[i]); }
    }
    auto list_one = [&](const fs::directory_entry& e){
        std::string name = e.path().filename().string();
        if(!all && name.size()>0 && name[0]=='.') return;
        if(longfmt){
            std::string type = e.is_directory()?"d":(e.is_symlink()?"l":"-");
            std::error_code ec; auto sz = e.is_regular_file()? e.file_size(ec):0; (void)ec;
            std::cout << type
                      << perm_string(fs::status(e.path()).permissions()) << ' '
                      << std::setw(8) << owner_string(e.path()) << ' '
                      << std::setw(10) << (e.is_regular_file()?human_size(sz):std::string("-")) << ' '
                      << time_string(fs::last_write_time(e, ec)) << ' '
                      << ' ' << name;
            if(e.is_symlink()){
                std::error_code ec2; auto tgt = fs::read_symlink(e.path(), ec2);
                if(!ec2) std::cout << " -> " << tgt.string();
            }
            std::cout << "\n";
        }else{
            std::cout << name << "\n";
        }
    };

    std::error_code ec;
    if(!fs::exists(target, ec)){ std::cerr << "ls: no such file or directory: "<< target <<"\n"; return; }
    if(fs::is_directory(target, ec)){
        if(tree){
            std::function<void(const fs::path&,int)> rec;
            rec = [&](const fs::path& p, int d){
                if(d<0) return;
                std::vector<fs::directory_entry> entries;
                for(auto& e: fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) entries.push_back(e);
                std::sort(entries.begin(), entries.end(), [](auto&a, auto&b){return a.path().filename()<b.path().filename();});
                for(auto& e: entries){
                    std::string name = e.path().filename().string();
                    if(!all && !name.empty() && name[0]=='.') continue;
                    std::cout << p.string() << "/" << name << "\n";
                    if(e.is_directory()) rec(e.path(), d-1);
                }
            };
            rec(target, depth);
        }else{
            std::vector<fs::directory_entry> entries;
            for(auto& e: fs::directory_iterator(target, fs::directory_options::skip_permission_denied, ec)) entries.push_back(e);
            std::sort(entries.begin(), entries.end(), [](auto&a, auto&b){return a.path().filename()<b.path().filename();});
            for(auto& e: entries) list_one(e);
        }
    }else{
        list_one(fs::directory_entry(target));
    }
}

void cmd_cd(Ctx& ctx, const std::vector<std::string>& args){
    fs::path dest = (args.empty()? fs::path(getenv("HOME")?getenv("HOME"):"/") : fs::path(args[0]));
    std::error_code ec; fs::path newp = fs::weakly_canonical((dest.is_absolute()?dest:(ctx.cwd/dest)), ec);
    if(ec || !fs::exists(newp)) { std::cerr << "cd: no such directory: "<< dest <<"\n"; return; }
    if(!fs::is_directory(newp)) { std::cerr << "cd: not a directory: "<< dest <<"\n"; return; }
    ctx.cwd = newp;
}

bool confirm(const std::string& prompt, bool force){
    if(force) return true;
    std::cout << prompt << " [y/N]: ";
    std::string ans; std::getline(std::cin, ans);
    return (!ans.empty() && (ans[0]=='y' || ans[0]=='Y'));
}

void copy_file_or_dir(const fs::path& src, const fs::path& dst){
    std::error_code ec;
    if(fs::is_directory(src)){
        fs::create_directories(dst, ec);
        for(auto& e: fs::recursive_directory_iterator(src, fs::directory_options::skip_permission_denied, ec)){
            auto rel = fs::relative(e.path(), src, ec);
            auto out = dst / rel;
            if(e.is_directory()) fs::create_directories(out, ec);
            else if(e.is_symlink()){
                std::error_code ec2; auto target = fs::read_symlink(e.path(), ec2);
                if(!ec2){ fs::create_directories(out.parent_path(), ec); fs::create_symlink(target, out, ec); }
            }else{
                fs::create_directories(out.parent_path(), ec);
                fs::copy_file(e.path(), out, fs::copy_options::overwrite_existing, ec);
            }
        }
    }else{
        fs::create_directories(dst.parent_path(), ec);
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if(ec) throw std::system_error(ec);
    }
}

void cmd_cp(const Ctx& ctx, const std::vector<std::string>& args){
    if(args.size()<2){ std::cerr << "cp: usage: cp <src> <dst>\n"; return; }
    fs::path src = ctx.cwd/args[0]; fs::path dst = ctx.cwd/args[1];
    std::error_code ec; src = fs::weakly_canonical(src, ec);
    if(ec || !fs::exists(src)){ std::cerr << "cp: cannot stat '"<<args[0]<<"'\n"; return; }
    if(fs::is_directory(dst) || (!dst.has_filename())) dst /= src.filename();
    try{ copy_file_or_dir(src, dst); }
    catch(const std::exception& e){ std::cerr << "cp: "<< e.what() <<"\n"; }
}

void remove_path(const fs::path& p){ std::error_code ec; fs::remove_all(p, ec); if(ec) throw std::system_error(ec); }

void cmd_rm(const Ctx& ctx, const std::vector<std::string>& args){
    if(args.empty()){ std::cerr << "rm: usage: rm <path> [more...]\n"; return; }
    for(const auto& a: args){ fs::path p = ctx.cwd/a; std::error_code ec; if(!fs::exists(p, ec)){ std::cerr<<"rm: no such file or directory: "<<a<<"\n"; continue; }
        if(confirm("Delete '"+p.string()+"' recursively?", ctx.force)){
            try{ remove_path(p);}catch(const std::exception& e){ std::cerr<<"rm: "<<e.what()<<"\n"; }
        }
    }
}

void cmd_mv(const Ctx& ctx, const std::vector<std::string>& args){
    if(args.size()<2){ std::cerr << "mv: usage: mv <src> <dst>\n"; return; }
    std::error_code ec;
    fs::path src = fs::weakly_canonical(ctx.cwd/args[0], ec);
    if(ec || !fs::exists(src)) { std::cerr << "mv: cannot stat '"<<args[0]<<"'\n"; return; }
    fs::path dst = ctx.cwd/args[1];
    if(fs::is_directory(dst, ec)) dst /= src.filename();
    fs::create_directories(dst.parent_path(), ec);
    fs::rename(src, dst, ec);
    if(ec){
        try{ copy_file_or_dir(src, dst); remove_path(src);}catch(const std::exception& e){ std::cerr<<"mv: "<<e.what()<<"\n"; }
    }
}

void cmd_mkdir(const Ctx& ctx, const std::vector<std::string>& args){
    if(args.empty()){ std::cerr << "mkdir: usage: mkdir <dir> [more...]\n"; return; }
    for(const auto& a: args){ std::error_code ec; fs::create_directories((ctx.cwd/a), ec); if(ec) std::cerr<<"mkdir: "<<ec.message()<<"\n"; }
}

void cmd_touch(const Ctx& ctx, const std::vector<std::string>& args){
    if(args.empty()){ std::cerr << "touch: usage: touch <file> [more...]\n"; return; }
    for(const auto& a: args){ fs::path p = ctx.cwd/a; std::error_code ec; if(!fs::exists(p, ec)) { std::ofstream(p.string()).close(); }
        auto now = fs::file_time_type::clock::now(); fs::last_write_time(p, now, ec); if(ec) std::cerr<<"touch: "<<ec.message()<<"\n"; }
}

void cmd_cat(const Ctx& ctx, const std::vector<std::string>& args){
    if(args.empty()){ std::cerr << "cat: usage: cat <file>\n"; return; }
    fs::path p = ctx.cwd/args[0]; std::ifstream f(p);
    if(!f){ std::cerr<<"cat: cannot open file\n"; return; }
    std::cout << f.rdbuf();
}

void cmd_find(const Ctx& ctx, const std::vector<std::string>& args){
    fs::path root = ctx.cwd; std::string pattern = ".*"; bool regex_mode=false;
    for(size_t i=0;i<args.size();++i){
        if(args[i]=="-r") { regex_mode=true; }
        else if(args[i].rfind("--in=",0)==0) { root = fs::path(args[i].substr(5)); }
        else { pattern = args[i]; }
    }
    std::error_code ec;
    std::regex re; if(regex_mode) re = std::regex(pattern);
    for(auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec); it!=fs::recursive_directory_iterator(); ++it){
        if(g_should_exit) { std::cerr << "find: interrupted\n"; return; }
        auto name = it->path().filename().string();
        bool match=false;
        if(regex_mode) { match = std::regex_search(name, re); }
        else { match = (name.find(pattern)!=std::string::npos); }
        if(match){ std::cout << it->path().string() << "\n"; }
    }
}

void cmd_chmod(const Ctx& ctx, const std::vector<std::string>& args){
    if(args.size()<2){ std::cerr << "chmod: usage: chmod <octal|+x|-w|...> <path> [more...]\n"; return; }
    std::string mode = args[0];
    auto apply_symbolic = [&](fs::perms cur, char who, char op, const std::string& perms){
        auto setbits=[&](char p){
            switch(p){
                case 'r': return std::array<fs::perms,3>{fs::perms::owner_read, fs::perms::group_read, fs::perms::others_read};
                case 'w': return std::array<fs::perms,3>{fs::perms::owner_write, fs::perms::group_write, fs::perms::others_write};
                case 'x': return std::array<fs::perms,3>{fs::perms::owner_exec, fs::perms::group_exec, fs::perms::others_exec};
                default:  return std::array<fs::perms,3>{fs::perms::none, fs::perms::none, fs::perms::none};
            }
        };
        int idx = (who=='u'?0:(who=='g'?1:(who=='o'?2:3)));
        if(idx==3){
            fs::perms bits = fs::perms::none; for(char c: perms){ auto arr=setbits(c); bits|=arr[0]|arr[1]|arr[2]; }
            if(op=='+') cur |= bits; else if(op=='-') cur &= fs::perms(~(unsigned(bits)));
        }else{
            fs::perms bits = fs::perms::none; for(char c: perms){ auto arr=setbits(c); bits|=arr[idx]; }
            if(op=='+') cur |= bits; else if(op=='-') cur &= fs::perms(~(unsigned(bits)));
        }
        return cur;
    };

    auto parse_octal=[&](const std::string& s)->std::optional<fs::perms>{
        if(s.size()!=3 || !std::all_of(s.begin(), s.end(), ::isdigit)) return std::nullopt;
        int u=s[0]-'0', g=s[1]-'0', o=s[2]-'0';
        fs::perms res = fs::perms::none;
        if(u&4) { res|=fs::perms::owner_read; }
        if(u&2) { res|=fs::perms::owner_write; }
        if(u&1) { res|=fs::perms::owner_exec; }
        if(g&4) { res|=fs::perms::group_read; }
        if(g&2) { res|=fs::perms::group_write; }
        if(g&1) { res|=fs::perms::group_exec; }
        if(o&4) { res|=fs::perms::others_read; }
        if(o&2) { res|=fs::perms::others_write; }
        if(o&1) { res|=fs::perms::others_exec; }
        return res;
    };

    for(size_t i=1;i<args.size();++i){
        fs::path p = ctx.cwd/args[i]; std::error_code ec; auto st = fs::status(p, ec); if(ec){ std::cerr<<"chmod: "<<ec.message()<<"\n"; continue; }
        fs::perms newp = st.permissions();
        if(auto oct=parse_octal(mode)){
            newp = *oct;
        }else{
            std::stringstream ss(mode); std::string tok;
            while(std::getline(ss, tok, ',')){
                if(tok.size()<3) { continue; }
                char who = tok[0]; char op = tok[1]; std::string pr = tok.substr(2);
                newp = apply_symbolic(newp, who, op, pr);
            }
        }
        fs::permissions(p, newp, ec);
        if(ec) std::cerr<<"chmod: "<<ec.message()<<"\n";
    }
}

void cmd_info(const Ctx& ctx, const std::vector<std::string>& args){
    fs::path p = (args.empty()? ctx.cwd : ctx.cwd/args[0]);
    std::error_code ec; auto stat = fs::status(p, ec); if(ec){ std::cerr<<"info: "<<ec.message()<<"\n"; return; }
    std::cout << "Path: " << fs::weakly_canonical(p, ec) << "\n";
    std::cout << "Type: " << (fs::is_directory(stat)?"directory":fs::is_regular_file(stat)?"file":fs::is_symlink(stat)?"symlink":"other") << "\n";
    if(fs::is_regular_file(stat)){
        std::cout << "Size: "; auto sz = fs::file_size(p, ec); if(!ec) std::cout << human_size(sz) << " ("<< sz <<" bytes)\n"; else std::cout<<"-\n";
    }
    std::cout << "Perms: " << perm_string(stat.permissions()) << "\n";
    std::cout << "Owner: " << owner_string(p) << "\n";
}

void print_help(){
    std::cout << R"HELP(
Commands:
  pwd                              - print current directory
  ls [-a] [-l] [--tree] [--depth=N] [path]
  cd [path]                        - change directory (default: $HOME)
  cp <src> <dst>                   - copy file/dir (recursive)
  mv <src> <dst>                   - move/rename
  rm <path> [more...]              - remove file/dir (recursive, asks to confirm)
  mkdir <dir> [more...]            - create directories (parents as needed)
  touch <file> [more...]           - create/update files
  cat <file>                       - print file content
  find [pattern] [-r] [--in=dir]   - search by substring (default) or regex (-r)
  chmod <octal|spec> <path> [...]  - change permissions, e.g., 755 or u+x,g-w,a+r
  info [path]                      - show metadata (size, perms, owner)
  tree [path] [--depth=N]          - alias for ls --tree [--depth=N]
  force on|off                     - toggle destructive operations confirmation
  help                             - this help
  exit / quit                      - leave program
)HELP";
}

int main(int argc, char** argv){
    std::signal(SIGINT, handle_sigint);
    Ctx ctx; ctx.force=false; ctx.cwd = (argc>1? fs::path(argv[1]) : fs::current_path());
    std::error_code ec; ctx.cwd = fs::weakly_canonical(ctx.cwd, ec);
    if(ec){ std::cerr << "Cannot access start directory: "<< (argc>1?argv[1]:".") <<"\n"; return 1; }

    std::cout << "Console File Explorer — starting in " << ctx.cwd << "\nType 'help' for commands. Ctrl+C to interrupt long ops.\n";

    std::string line;
    while(true){
        if(g_should_exit){ std::cout << "\nInterrupted. Type 'exit' to quit.\n"; g_should_exit=0; }
        std::cout << "["<< ctx.cwd << "]$ " << std::flush;
        if(!std::getline(std::cin, line)) break;
        if(line.empty()) continue;

        std::istringstream iss(line); std::string cmd; iss >> cmd; std::vector<std::string> args; std::string a;
        while(iss >> a) { if(!a.empty()) args.push_back(a); }

        if(cmd=="pwd") cmd_pwd(ctx);
        else if(cmd=="ls") cmd_ls(ctx, args);
        else if(cmd=="tree"){ auto a2=args; a2.insert(a2.begin(), "--tree"); cmd_ls(ctx, a2); }
        else if(cmd=="cd") cmd_cd(ctx, args);
        else if(cmd=="cp") cmd_cp(ctx, args);
        else if(cmd=="mv") cmd_mv(ctx, args);
        else if(cmd=="rm") cmd_rm(ctx, args);
        else if(cmd=="mkdir") cmd_mkdir(ctx, args);
        else if(cmd=="touch") cmd_touch(ctx, args);
        else if(cmd=="cat") cmd_cat(ctx, args);
        else if(cmd=="find") cmd_find(ctx, args);
        else if(cmd=="chmod") cmd_chmod(ctx, args);
        else if(cmd=="info") cmd_info(ctx, args);
        else if(cmd=="force"){
            if(args.empty()) std::cout << "force is "<< (ctx.force?"on":"off") <<"\n";
            else if(args[0]=="on") ctx.force=true; else if(args[0]=="off") ctx.force=false; else std::cerr<<"usage: force on|off\n";
        }
        else if(cmd=="help") print_help();
        else if(cmd=="exit" || cmd=="quit") break;
        else std::cerr << cmd << ": command not found (type 'help')\n";
    }
    return 0;
}
