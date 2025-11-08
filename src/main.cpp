#include <bits/stdc++.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
using namespace std;

// ===== Utils =====
static inline string trim(const string &s){ size_t a=s.find_first_not_of(" \t\r\n"); if(a==string::npos) return ""; size_t b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1); }
static inline string lower(string s){ for(char &c:s) c=tolower(c); return s; }
static inline string json_escape(const string& s){ string out; out.reserve(s.size()+8); for(char c: s){ if(c=='\\' || c=='"') { out.push_back('\\'); out.push_back(c);} else if(c=='\n') { out+="\\n"; } else out.push_back(c);} return out; }

// ===== Global CPU =====
struct CpuTimes{ unsigned long long user=0,nice_=0,system=0,idle=0,iowait=0,irq=0,softirq=0,steal=0; };
bool read_total_cpu(CpuTimes &t){ ifstream f("/proc/stat"); if(!f) return false; string id; f>>id; if(id!="cpu") return false; f>>t.user>>t.nice_>>t.system>>t.idle>>t.iowait>>t.irq>>t.softirq>>t.steal; return true; }
struct CpuSampler{ CpuTimes prev{}; bool primed=false; };
double compute_total_cpu(CpuSampler &cs){
    CpuTimes cur; if(!read_total_cpu(cur)) return 0.0;
    if(!cs.primed){ cs.prev=cur; cs.primed=true; return 0.0; }
    unsigned long long idle1=cs.prev.idle+cs.prev.iowait, idle2=cur.idle+cur.iowait;
    unsigned long long non1 =cs.prev.user+cs.prev.nice_+cs.prev.system+cs.prev.irq+cs.prev.softirq+cs.prev.steal;
    unsigned long long non2 =cur.user+cur.nice_+cur.system+cur.irq+cur.softirq+cur.steal;
    double totald=(idle2+non2)-(idle1+non1), idled=(idle2-idle1);
    cs.prev=cur; if(totald<=0) return 0.0; return 100.0*((totald-idled)/totald);
}

// ===== Memory =====
struct MemInfo{ long long totalKB=0, availKB=0; };
MemInfo read_meminfo(){ MemInfo m; ifstream f("/proc/meminfo"); string k,unit; long long v; while(f>>k>>v>>unit){ if(k=="MemTotal:") m.totalKB=v; else if(k=="MemAvailable:") m.availKB=v; } return m; }

// ===== Per-process =====
struct ProcSample{ unsigned long long utime=0, stime=0, totalJiffies=0; };
struct ProcRow{ int pid=0; string name; double cpu=0.0; long long memKB=0; };

bool read_pid_stat(int pid, unsigned long long &ut, unsigned long long &st, string &comm){
    string path="/proc/"+to_string(pid)+"/stat"; ifstream f(path); if(!f) return false;
    string line; getline(f,line);
    size_t L=line.find('('), R=line.rfind(')'); if(L==string::npos||R==string::npos||R<=L) return false;
    comm=line.substr(L+1,R-L-1);
    vector<string> parts; { string rest=line.substr(R+2); string tmp; stringstream ss(rest); while(ss>>tmp) parts.push_back(tmp); }
    if(parts.size()<15) return false;
    ut=stoull(parts[11]); st=stoull(parts[12]); // fields 14 & 15
    return true;
}
long long read_pid_vmrss_kb(int pid){ string path="/proc/"+to_string(pid)+"/status"; ifstream f(path); if(!f) return 0; string k,unit; long long v; while(f>>k>>v>>unit){ if(k=="VmRSS:") return v; } return 0; }
unsigned long long read_total_jiffies(){ CpuTimes t; if(!read_total_cpu(t)) return 0ULL; return t.user+t.nice_+t.system+t.idle+t.iowait+t.irq+t.softirq+t.steal; }
vector<int> list_pids(){ vector<int> pids; DIR* d=opendir("/proc"); if(!d) return pids; dirent* e; while((e=readdir(d))) if(isdigit(e->d_name[0])) pids.push_back(atoi(e->d_name)); closedir(d); return pids; }

struct Sampler{
    unordered_map<int,ProcSample> prev;
    double proc_cpu(int pid, unsigned long long ut, unsigned long long st){
        unsigned long long nowTotal=read_total_jiffies();
        auto it=prev.find(pid);
        unsigned long long dProc=0, dTotal=1;
        if(it!=prev.end()){ dProc=(ut+st)-(it->second.utime+it->second.stime); dTotal=max(1ULL, nowTotal - it->second.totalJiffies); }
        prev[pid]={ut,st,nowTotal};
        return 100.0*((double)dProc/(double)dTotal);
    }
};

// ===== Shared snapshot (for REST) =====
struct Snapshot{
    double cpuTotal=0.0;
    double memUsedGB=0.0, memTotalGB=0.0;
    int procCount=0;
    vector<ProcRow> top; // sorted
};
std::mutex SNAP_MTX;
Snapshot SNAP;

// ====== TUI helpers ======
void clear_screen(){ cout<<"\033[2J\033[H"; }
void hide_cursor(){ cout<<"\033[?25l"; }
void show_cursor(){ cout<<"\033[?25h"; }

struct State{ string sortBy="cpu"; string filter; };

void draw_header(double cpuPct, const MemInfo &mi){
    double usedGB=(mi.totalKB-mi.availKB)/(1024.0*1024.0);
    double totalGB= mi.totalKB/(1024.0*1024.0);
    cout<<"System Monitor (C++ /proc TUI)\n";
    cout<<"--------------------------------\n";
    cout.setf(std::ios::fixed); cout<<setprecision(1)<<"CPU: "<<cpuPct<<"%";
    cout.setf(std::ios::fixed); cout<<setprecision(2)<<"   MEM: "<<usedGB<<" / "<<totalGB<<" GB\n";
}
void draw_table(const vector<ProcRow>& rows, const State& st){
    cout<<"\nPID      NAME                            CPU%    MEM(MB)\n";
    cout<<"--------------------------------------------------------\n";
    for(const auto &r: rows){
        if(!st.filter.empty()){
            string ln=lower(r.name);
            if(ln.find(lower(st.filter))==string::npos) continue;
        }
        cout.setf(std::ios::fixed); cout<<setprecision(1);
        cout<<left<<setw(8)<<r.pid
            <<left<<setw(32)<<r.name.substr(0,31)
            <<right<<setw(7)<<r.cpu
            <<right<<setw(10)<<(r.memKB/1024.0)<<"\n";
    }
}
void draw_footer(const State& st, bool apiOn){
    cout<<"\nCommands: q | s cpu | s mem | f <text> | f | k <pid> | k! <pid> | help\n";
    cout<<"API: "<<(apiOn? "http://127.0.0.1:8080  ":"off   ")<<"  Sort:"<<st.sortBy<<"  Filter:'"<<st.filter<<"'\n> "<<flush;
}
bool parse_kill(const string &cmd, bool &force, int &pid){
    string t=trim(cmd);
    if(t.rfind("k!",0)==0) force=true;
    else if(t.rfind("k",0)==0) force=false;
    else return false;
    string rest=trim(t.substr(force?2:1)); if(rest.empty()) return false; pid=stoi(rest); return true;
}

// ===== Minimal HTTP server =====
string http_ok_json(const string& body){
    ostringstream oss;
    oss<<"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\n\r\n"<<body;
    return oss.str();
}
string http_ok_html(const string& body){
    ostringstream oss;
    oss<<"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\n\r\n"<<body;
    return oss.str();
}
string http_notfound(){
    string b="<h1>404</h1>";
    ostringstream oss; oss<<"HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: "<<b.size()<<"\r\nConnection: close\r\n\r\n"<<b;
    return oss.str();
}
string make_metrics_json(){
    lock_guard<mutex> lk(SNAP_MTX);
    ostringstream j; j.setf(std::ios::fixed); j<<setprecision(1);
    j<<"{"
      <<"\"cpu_total\":"<<SNAP.cpuTotal<<","
      <<"\"mem_used_gb\":"<<SNAP.memUsedGB<<","
      <<"\"mem_total_gb\":"<<SNAP.memTotalGB<<","
      <<"\"processes\":"<<SNAP.procCount
      <<"}";
    return j.str();
}
string make_procs_json(int limit=50){
    lock_guard<mutex> lk(SNAP_MTX);
    ostringstream j; j.setf(std::ios::fixed); j<<setprecision(1);
    j<<"{\"top_processes\":[";
    int n=min<int>(limit, SNAP.top.size());
    for(int i=0;i<n;i++){
        const auto &p=SNAP.top[i];
        if(i) j<<",";
        j<<"{\"pid\":"<<p.pid<<",\"name\":\""<<json_escape(p.name)<<"\",\"cpu\":"<<p.cpu<<",\"mem_mb\":"<<setprecision(2)<<(p.memKB/1024.0)<<"}";
        j<<setprecision(1);
    }
    j<<"]}";
    return j.str();
}

// Embedded dashboard (single file, uses relative /api/*)
string dashboard_html(){
    return R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>SysMon Dashboard</title>
<style>
:root { --bg:#0b1220; --card:#11192b; --fg:#e6edf6; --muted:#9fb0c3; --ok:#29b36b; --warn:#f0b429; --bad:#e5534b; }
html,body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.4 system-ui,Segoe UI,Roboto,Ubuntu,sans-serif}
.wrap{max-width:980px;margin:24px auto;padding:0 16px}
.row{display:flex;gap:16px;flex-wrap:wrap}
.card{background:var(--card);border-radius:14px;box-shadow:0 6px 20px rgba(0,0,0,.25);padding:16px;flex:1}
.metrics{display:flex;gap:16px;flex-wrap:wrap}
.metric{background:#0f1730;padding:12px 14px;border-radius:12px;min-width:180px}
.label{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.06em}
.value{font-size:24px;margin-top:6px;font-weight:700}
.status{display:inline-block;padding:2px 8px;border-radius:999px;font-size:12px;margin-left:8px}
.ok{background:rgba(41,179,107,.16);color:var(--ok)}
.warn{background:rgba(240,180,41,.16);color:var(--warn)}
.bad{background:rgba(229,83,75,.16);color:var(--bad)}
table{width:100%;border-collapse:collapse;margin-top:10px}
th,td{padding:10px 8px;border-bottom:1px solid rgba(255,255,255,.06)}
th{color:var(--muted);text-align:left;font-weight:600}
tr:hover{background:rgba(255,255,255,.03)}
.right{text-align:right}
.footer{color:var(--muted);opacity:.8;margin-top:10px}
code{color:#79c0ff}
</style></head>
<body><div class="wrap">
  <h2>System Monitor — Web Dashboard</h2>
  <div class="row"><div class="card">
    <div class="metrics">
      <div class="metric"><div class="label">CPU Total</div><div class="value" id="cpu">– % <span id="cpuState" class="status ok">idle</span></div></div>
      <div class="metric"><div class="label">Memory Used</div><div class="value"><span id="memUsed">–</span> / <span id="memTotal">–</span> GB</div></div>
      <div class="metric"><div class="label">Processes</div><div class="value" id="procs">–</div></div>
      <div class="metric"><div class="label">API</div><div class="value"><code id="apiOrigin"></code></div></div>
    </div>
    <div class="footer">Auto-refreshes every 1s. Keep <code>sysmon --api</code> running.</div>
  </div></div>

  <div class="row"><div class="card">
    <div class="label">Top Processes</div>
    <table><thead><tr><th>PID</th><th>Name</th><th class="right">CPU %</th><th class="right">Mem (MB)</th></tr></thead>
      <tbody id="tbody"></tbody>
    </table>
  </div></div>

  <div class="footer">Tip: In another tab run <code>yes &gt; /dev/null</code> to see CPU change.</div>
</div>

<script>
const $=(id)=>document.getElementById(id);
function badge(el,v){ el.classList.remove('ok','warn','bad'); if(v<40) el.classList.add('ok'),el.textContent='idle'; else if(v<80) el.classList.add('warn'),el.textContent='busy'; else el.classList.add('bad'),el.textContent='hot'; }
async function j(u){ const r=await fetch(u,{cache:'no-store'}); if(!r.ok) throw new Error(r.status); return r.json(); }
async function tick(){
  try{
    $('apiOrigin').textContent = location.origin;
    const [m,p]=await Promise.all([j('/api/metrics'), j('/api/procs')]);
    const cpu=(m.cpu_total||0).toFixed(1);
    $('cpu').firstChild.nodeValue = cpu+' % ';
    badge($('cpuState'), parseFloat(cpu));
    $('memUsed').textContent = (m.mem_used_gb||0).toFixed(2);
    $('memTotal').textContent = (m.mem_total_gb||0).toFixed(2);
    $('procs').textContent   = (m.processes||0);
    const tb=$('tbody'); tb.innerHTML='';
    (p.top_processes||[]).slice(0,50).forEach(r=>{
      const tr=document.createElement('tr');
      tr.innerHTML = `<td>${r.pid}</td><td>${r.name}</td><td class="right">${(r.cpu||0).toFixed(1)}</td><td class="right">${(r.mem_mb||0).toFixed(2)}</td>`;
      tb.appendChild(tr);
    });
  }catch(e){ console.error(e); }
}
setInterval(tick,1000); tick();
</script></body></html>)HTML";
}

// HTTP server thread
void run_http_server_atomic(bool *stopFlag){
    int srv=socket(AF_INET, SOCK_STREAM, 0);
    if(srv<0){ perror("socket"); return; }
    int opt=1; setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(8080); addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(srv,(sockaddr*)&addr,sizeof(addr))<0){ perror("bind"); close(srv); return; }
    if(listen(srv,16)<0){ perror("listen"); close(srv); return; }

    while(!*stopFlag){
        fd_set rf; FD_ZERO(&rf); FD_SET(srv,&rf);
        timeval tv{0,200000}; // 200ms
        int rv=select(srv+1,&rf,nullptr,nullptr,&tv);
        if(rv<=0) continue;
        int c=accept(srv,nullptr,nullptr);
        if(c<0) continue;

        char buf[4096]; int n=read(c,buf,sizeof(buf)-1); if(n<0){ close(c); continue; } buf[n]=0;
        string req(buf), path="/"; { istringstream ss(req); string method,http; ss>>method>>path>>http; }

        string resp;
        if(path=="/" || path=="/dashboard") resp = http_ok_html(dashboard_html());
        else if(path=="/api/metrics")       resp = http_ok_json(make_metrics_json());
        else if(path=="/api/procs")         resp = http_ok_json(make_procs_json());
        else                                resp = http_notfound();

        write(c, resp.data(), resp.size());
        close(c);
    }
    close(srv);
}

// ===== Main =====
int main(int argc, char** argv){
    bool apiMode=false;
    for(int i=1;i<argc;i++){ string a=argv[i]; if(a=="--api") apiMode=true; }

    ios::sync_with_stdio(false); cin.tie(nullptr);
    hide_cursor(); atexit(show_cursor);

    // state
    struct State{ string sortBy="cpu"; string filter; } st;
    Sampler ps; CpuSampler cs;

    // HTTP server thread
    bool stopServer=false; thread httpThread;
    if(apiMode){ httpThread=thread(run_http_server_atomic, &stopServer); }

    // TUI loop
    while(true){
        vector<int> pids=list_pids();
        vector<ProcRow> rows; rows.reserve(pids.size());
        for(int pid: pids){
            unsigned long long ut=0, stt=0; string name;
            if(!read_pid_stat(pid, ut, stt, name)) continue;
            double cpu=ps.proc_cpu(pid, ut, stt);
            long long memKB=read_pid_vmrss_kb(pid);
            rows.push_back({pid,name,cpu,memKB});
        }
        if(st.sortBy=="mem") sort(rows.begin(), rows.end(), [](auto&a, auto&b){return a.memKB>b.memKB;});
        else                 sort(rows.begin(), rows.end(), [](auto&a, auto&b){return a.cpu>b.cpu;});
        if(rows.size()>500) rows.resize(500);

        auto mi=read_meminfo();
        double cpuTot=compute_total_cpu(cs);
        double usedGB=(mi.totalKB-mi.availKB)/(1024.0*1024.0);
        double totalGB= mi.totalKB/(1024.0*1024.0);

        if(apiMode){
            lock_guard<mutex> lk(SNAP_MTX);
            SNAP.cpuTotal=cpuTot; SNAP.memUsedGB=usedGB; SNAP.memTotalGB=totalGB;
            SNAP.procCount=(int)pids.size(); SNAP.top=rows;
        }

        clear_screen();
        cout<<"System Monitor (C++ /proc TUI)\n--------------------------------\n";
        cout.setf(std::ios::fixed); cout<<setprecision(1)<<"CPU: "<<cpuTot<<"%";
        cout.setf(std::ios::fixed); cout<<setprecision(2)<<"   MEM: "<<usedGB<<" / "<<totalGB<<" GB\n";
        cout<<"\nPID      NAME                            CPU%    MEM(MB)\n";
        cout<<"--------------------------------------------------------\n";
        for(const auto &r: rows){
            if(!st.filter.empty()){ string ln=lower(r.name); if(ln.find(lower(st.filter))==string::npos) continue; }
            cout.setf(std::ios::fixed); cout<<setprecision(1);
            cout<<left<<setw(8)<<r.pid<<left<<setw(32)<<r.name.substr(0,31)
                <<right<<setw(7)<<r.cpu<<right<<setw(10)<<(r.memKB/1024.0)<<"\n";
        }
        cout<<"\nCommands: q | s cpu | s mem | f <text> | f | k <pid> | k! <pid> | help\n";
        cout<<"API: "<<(apiMode? "http://127.0.0.1:8080  ":"off   ")<<"  Sort:"<<st.sortBy<<"  Filter:'"<<st.filter<<"'\n> "<<flush;

        fd_set set; FD_ZERO(&set); FD_SET(0,&set);
        timeval tv{1,0};
        int rv=select(1,&set,nullptr,nullptr,&tv);
        if(rv>0){
            string cmd; getline(cin, cmd);
            string t=trim(cmd);
            if(t=="q") break;
            else if(t=="s cpu") st.sortBy="cpu";
            else if(t=="s mem") st.sortBy="mem";
            else if(t=="f") st.filter.clear();
            else if(t.rfind("f ",0)==0) st.filter=t.substr(2);
            else if(t=="help"){ cout<<"\nPress Enter..."; string d; getline(cin,d); }
            else { bool force=false; int kpid=0; if(parse_kill(t,force,kpid)){ kill(kpid, force?SIGKILL:SIGTERM); } }
        }
    }

    if(apiMode){ stopServer=true; if(httpThread.joinable()) httpThread.join(); }
    clear_screen(); show_cursor(); cout<<"Bye!\n"; return 0;
}
