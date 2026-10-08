#!/usr/bin/env python3
"""Compile the production scope router against counted UI/VFS doubles."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SUPPORT = r'''
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>
struct idStr {
    std::string value;
    idStr(const char* v=""):value(v){}
    operator const char*()const{return value.c_str();}
    const char* c_str()const{return value.c_str();}
    static int Icmp(const char*a,const char*b){std::string x(a),y(b);for(auto&c:x)c=std::tolower(c);for(auto&c:y)c=std::tolower(c);return x.compare(y);}
    int Icmp(const char*b)const{return Icmp(value.c_str(),b);}
    void SetFileExtension(const char*ext){value=value.substr(0,value.find_last_of('.'))+ext;}
    void Clear(){value.clear();}
};
struct idKeyValue {idStr key,value;const idStr& GetKey()const{return key;}const idStr& GetValue()const{return value;}};
struct idDict {
    std::vector<idKeyValue> values;
    void Set(const char*k,const char*v){for(auto&x:values)if(!x.key.Icmp(k)){x.value=v;return;}values.push_back({k,v});}
    const char* GetString(const char*k)const{for(auto&x:values)if(!x.key.Icmp(k))return x.value;return "";}
    int GetNumKeyVals()const{return (int)values.size();}
    const idKeyValue*GetKeyVal(int i)const{return &values[i];}
};
struct idUserInterface {
    idStr name;idDict state;bool unique=false;int changed=0;std::vector<std::string> events;
    idUserInterface(const char*n):name(n){state.Set("name",n);}
    const char*Name()const{return name;} const idDict&State()const{return state;}
    void SetStateString(const char*k,const char*v){state.Set(k,v);}
    void StateChanged(int time){changed=time;} float GetStateFloat(const char*k)const{return std::atof(state.GetString(k));}
    void HandleNamedEvent(const char*e){events.push_back(e);} bool IsUniqued()const{return unique;}
};
struct Cvars {bool retained=true;const char*mod="baseoq4";bool GetCVarBool(const char*){return retained;}const char*GetCVarString(const char*k){return std::string(k)=="fs_game"?mod:"";}} cv;
auto*cvarSystem=&cv;
struct idFile{};
struct Files {bool missing=false,modOverride=false;int reads=0;idFile file;int ReadFile(const char*,void*){++reads;return missing?-1:1;}idFile*OpenFileRead(const char*,bool,const char*){return modOverride?&file:nullptr;}void CloseFile(idFile*){}} fs;
auto*fileSystem=&fs;
struct Manager {
    bool invalid=false;int loads=0,frees=0;
    idUserInterface*FindGui(const char*n,bool,bool unique){++loads;if(invalid)return nullptr;auto*g=new idUserInterface(n);g->unique=unique;return g;}
    void DeAlloc(idUserInterface*g){++frees;delete g;}
} manager;
auto*uiManager=&manager;
struct Game {int time=101;} gameLocal;
'''
MAIN = r'''
int main(){
 for(const char*kind:{"machinegun","railgun","nailgun"}){
    const std::string stock=std::string("guis/weapons/")+kind+"_scope.gui",retained=stock.substr(0,stock.size()-4)+".q4ui";
    idDict args;args.Set("gui_zoom",stock.c_str());idStr requested;
    idUserInterface old(stock.c_str());old.SetStateString("lockStatus","1");old.SetStateString("lockRange","12.4");
    idUserInterface*gui=&old;cv.retained=false;
    OpenQ4_UpdateScopeGui(gui,requested,args);assert(gui==&old);
    cv.retained=true;OpenQ4_UpdateScopeGui(gui,requested,args);
    assert(gui!=&old && std::string(gui->Name())==retained && gui->changed==101 && gui->unique);
    assert(std::string(gui->State().GetString("name"))==retained && std::string(gui->State().GetString("lockRange"))=="12.4");
    assert(gui->events.size()==1 && gui->events[0]=="lockAquired");
    int loads=manager.loads,reads=fs.reads;OpenQ4_UpdateScopeGui(gui,requested,args);assert(manager.loads==loads && fs.reads==reads);
    // Restore has consumed the serialized GUI, then clears only the transient preference cache.
    requested.Clear();cv.retained=false;int frees=manager.frees;
    OpenQ4_UpdateScopeGui(gui,requested,args);assert(std::string(gui->Name())==stock && manager.frees==frees+1);
    manager.DeAlloc(gui);
    gui=&old;requested.Clear();cv.retained=true;fs.missing=true;
    OpenQ4_UpdateScopeGui(gui,requested,args);assert(gui==&old);fs.missing=false;
    requested.Clear();manager.invalid=true;OpenQ4_UpdateScopeGui(gui,requested,args);assert(gui==&old);manager.invalid=false;
    requested.Clear();cv.mod="custom";fs.modOverride=true;OpenQ4_UpdateScopeGui(gui,requested,args);assert(gui==&old);
    cv.mod="baseoq4";fs.modOverride=false;
 }
 idDict args;args.Set("gui_zoom","guis/modscope.gui");idStr requested;idUserInterface old("guis/modscope.gui");auto*gui=&old;
 int reads=fs.reads;OpenQ4_UpdateScopeGui(gui,requested,args);assert(gui==&old && reads==fs.reads);
}
'''

def main():
    subprocess.run([sys.executable, str(ROOT/'tools/ui/build_retained_scopes.py'), '--check'], cwd=ROOT, check=True)
    compiler=next((shutil.which(n) for n in ('clang++','g++','c++') if shutil.which(n)),None)
    if not compiler:
        raise RuntimeError('C++ compiler required')
    with tempfile.TemporaryDirectory(prefix='scope-routing-',dir=ROOT/'.tmp') as directory:
        source=Path(directory)/'routing.cpp'; binary=Path(directory)/'routing.exe'
        source.write_text(SUPPORT+'\n#include "'+(ROOT/'src/game/gamesys/RetainedScope.h').as_posix()+'"\n'+MAIN,encoding='utf-8')
        subprocess.run([compiler,'-std=c++17',str(source),'-o',str(binary)],check=True,cwd=ROOT)
        subprocess.run([str(binary)],check=True,cwd=ROOT)
    print('scope routing: stock/retained, gate changes, state, restore, ownership, missing/invalid documents and mod precedence passed')

if __name__=='__main__':
    main()
