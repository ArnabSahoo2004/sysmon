#include <bits/stdc++.h>
#include <dirent.h>
#include <unistd.h>
using namespace std;

struct CpuTimes { unsigned long long user=0,nice_=0,system=0,idle=0,iowait=0,irq=0,softirq=0,steal=0; };
bool read_cpu(CpuTimes &t){
    ifstream f("/proc/stat"); if(!f) return false; string cpu;
    f>>cpu>>t.user>>t.nice_>>t.system>>t.idle>>t.iowait>>t.irq>>t.softirq>>t.steal; return cpu=="cpu";
}
double cpu_usage_pct(){
    CpuTimes a,b; if(!read_cpu(a)) return 0.0; usleep(200000); if(!read_cpu(b)) return 0.0;
    unsigned long long idleA=a.idle+a.iowait, idleB=b.idle+b.iowait;
    unsigned long long nonA=a.user+a.nice_+a.system+a.irq+a.softirq+a.steal;
    unsigned long long nonB=b.user+b.nice_+b.system+b.irq+b.softirq+b.steal;
    double tot=(idleB+nonB)-(idleA+nonA), idle=(idleB-idleA);
    return tot<=0?0.0:100.0*((tot-idle)/tot);
}

struct MemInfo { long long totalKB=0, availKB=0; };
MemInfo read_mem(){
    MemInfo m; ifstream f("/proc/meminfo"); string k,unit; long long v;
    while(f>>k>>v>>unit){ if(k=="MemTotal:") m.totalKB=v; else if(k=="MemAvailable:") m.availKB=v; }
    return m;
}

int count_procs(){
    int n=0; DIR* d=opendir("/proc"); if(!d) return 0; dirent* e;
    while((e=readdir(d))) if(isdigit(e->d_name[0])) n++; closedir(d); return n;
}

int main(){
    ios::sync_with_stdio(false); cin.tie(nullptr);
    while(true){
        double cpu = cpu_usage_pct();
        auto mi = read_mem();
        double usedGB = (mi.totalKB - mi.availKB)/(1024.0*1024.0);
        double totalGB = mi.totalKB/(1024.0*1024.0);
        int procs = count_procs();

        cout << "\033[2J\033[H"; // clear
        cout << "Simple System Monitor (WSL/Linux)\n";
        cout << "---------------------------------\n";
        cout << fixed << setprecision(1) << "CPU Usage      : " << cpu << " %\n";
        cout << setprecision(2) << "Memory Used    : " << usedGB << " GB / " << totalGB << " GB\n";
        cout << "Process Count  : " << procs << "\n";
        cout << "\nPress Ctrl+C to exit.\n";
        sleep(1);
    }
}
