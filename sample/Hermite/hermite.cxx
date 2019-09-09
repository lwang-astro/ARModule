#include <iostream>
#include <fstream>
//#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <iomanip>
#include <cmath>
#include <cassert>

#define ASSERT(expr) assert(expr)
#define DATADUMP(x) abort()

#include "Common/io.h"
#include "AR/symplectic_integrator.h"
#include "Hermite/hermite_integrator.h"
#include "particle.h"
#include "hermite_perturber.h"
#include "ar_interaction.h"
#include "hermite_interaction.h"
#include "hermite_information.h"


using namespace H4;

int main(int argc, char **argv){

    //unsigned int oldcw;
    //fpu_fix_start(&oldcw);
    // initial parameters
    COMM::IOParamsContainer input_par_store;

    COMM::IOParams<int> print_width    (input_par_store, WRITE_WIDTH,     "print width of value"); //print width
    COMM::IOParams<int> print_precision(input_par_store, WRITE_PRECISION, "print digital precision"); //print digital precision
    COMM::IOParams<int> nstep_max      (input_par_store, 1000000, "number of maximum step for AR integration"); // maximum time step allown for tsyn integration
    COMM::IOParams<int> sym_order      (input_par_store, -6, "Symplectic integrator order, should be even number"); // symplectic integrator order
    COMM::IOParams<int> dt_min_power_index (input_par_store, 40, "power index to calculate mimimum hermite time step"); // power index to calculate minimum physical time step
    COMM::IOParams<double> energy_error (input_par_store, 1e-10,"relative energy error limit for AR"); // phase error requirement
    COMM::IOParams<double> time_error   (input_par_store, 0.0, "time synchronization absolute error limit for AR","default is 0.25*dt-min"); // time synchronization error
    COMM::IOParams<double> time_zero    (input_par_store, 0.0, "initial physical time");    // initial physical time
    COMM::IOParams<double> time_end     (input_par_store, 1.0, "ending physical time "); // ending physical time
    COMM::IOParams<double> dt_output    (input_par_store, 0.25, "output time interval"); // output time interval
    COMM::IOParams<double> dt_max       (input_par_store, 0.25, "maximum hermite time step"); // maximum physical time step
    COMM::IOParams<double> r_break      (input_par_store, 1e-3, "distance criterion for switching AR and Hermite"); // binary break criterion
    COMM::IOParams<double> r_search     (input_par_store, 5.0,  "neighbor search radius"); // neighbor search radius for AR
    COMM::IOParams<double> eta_4th      (input_par_store, 0.1,  "time step coefficient for 4th order"); // time step coefficient 
    COMM::IOParams<double> eta_2nd      (input_par_store, 0.001,"time step coefficient for 2nd order"); // time step coefficient for 2nd order
    COMM::IOParams<double> eps_sq       (input_par_store, 0.0,  "softerning parameter");    // softening parameter
    COMM::IOParams<double> G            (input_par_store, 1.0,  "gravitational constant");      // gravitational constant
    COMM::IOParams<double> slowdown_ref (input_par_store, 1e-6, "slowdown perturbation ratio reference"); // slowdown reference factor
    COMM::IOParams<double> slowdown_mass_ref (input_par_store, 0.0, "slowdowm mass reference","averaged mass"); // slowdown mass reference
    COMM::IOParams<double> slowdown_timescale_max (input_par_store, 0.0, "maximum timescale for maximum slowdown factor","time-end"); // slowdown timescale
    COMM::IOParams<std::string> filename_par (input_par_store, "", "filename to load manager parameters","input name"); // par dumped filename

    int copt;
    static struct option long_options[] = {
        {"time-start", required_argument, 0, 0},
        {"time-end", required_argument, 0, 't'},
        {"r-break", required_argument, 0, 'r'},
        {"energy-error",required_argument, 0, 'e'},
        {"time-error",required_argument, 0, 0},
        {"dt-max",required_argument, 0, 0},
        {"dt-min-power",required_argument, 0, 0},
        {"n-step-max",required_argument, 0, 0},
        {"eta-4th",required_argument, 0, 0},
        {"eta-2nd",required_argument, 0, 0},
        {"eps",required_argument, 0, 0},
        {"slowdown-ref",required_argument, 0, 0},
        {"slowdown-mass-ref",required_argument, 0, 0},
        {"slowdown-timescale-max",required_argument, 0, 0},
        {"print-width",required_argument, 0, 0},
        {"print-precision",required_argument, 0, 0},
        {"help",no_argument, 0, 'h'},
        {0,0,0,0}
    };
  
    int option_index;
    while ((copt = getopt_long(argc, argv, "t:r:R:k:G:e:o:p:h", long_options, &option_index)) != -1)
        switch (copt) {
        case 0:
#ifdef DEBUG
            std::cerr<<"option "<<option_index<<" "<<long_options[option_index].name<<" "<<optarg<<std::endl;
#endif
            switch (option_index) {
            case 0:
                time_zero.value = atof(optarg);
                break;
            case 4:
                time_error.value = atof(optarg);
                break;
            case 5:
                dt_max.value = atof(optarg);
                break;
            case 6:
                dt_min_power_index.value = atoi(optarg);
                break;
            case 7:
                nstep_max.value = atoi(optarg);
                break;
            case 8:
                eta_4th.value = atof(optarg);
                break;
            case 9:
                eta_2nd.value = atof(optarg);
                break;
            case 10:
                eps_sq.value = atof(optarg);
                break;
            case 11:
                slowdown_ref.value = atof(optarg);
                break;
            case 12:
                slowdown_mass_ref.value = atof(optarg);
                break;
            case 13:
                slowdown_timescale_max.value = atof(optarg);
                break;
            case 14:
                print_width.value = atof(optarg);
                break;
            case 15:
                print_precision.value = atoi(optarg);
                break;
            default:
                std::cerr<<"Unknown option. check '-h' for help.\n";
                abort();
            }
            break;
        case 't':
            time_end.value = atof(optarg);
            break;
        case 'r':
            r_break.value = atof(optarg);
            break;
        case 'R':
            r_search.value = atof(optarg);
            break;
        case 'k':
            sym_order.value = atoi(optarg);
            break;
        case 'G':
            G.value = atof(optarg);
            break;
        case 'e':
            energy_error.value = atof(optarg);
            break;
        case 'o':
            dt_output.value = atof(optarg);
            break;
        case 'p':
            filename_par.value = optarg;
            FILE* fpar_in;
            if( (fpar_in = fopen(filename_par.value.c_str(),"r")) == NULL) {
                fprintf(stderr,"Error: Cannot open file %s.\n", filename_par.value.c_str());
                abort();
            }
            input_par_store.readAscii(fpar_in);
            fclose(fpar_in);
            break;
        case 'h':
            std::cout<<"chain [option] data_filename\n"
                     <<"Input data file format: \n"
                     <<"  First   line:  number of particles(N)\n"
                     <<"  2-(N+1) line:  mass, x, y, z, vx, vy, vz\n"
                     <<"  last    line:  N_group, group_offset_index_lst[N_group], group_member_particle_index[N_member_total]\n"
                     <<"Options: (*) show defaulted values\n"
                     <<"          --dt-max       [Float]:  "<<dt_max<<"\n"
                     <<"          --dt-min-power [int]  :  "<<dt_min_power_index<<"\n"
                     <<"    -e [Float]:  "<<energy_error<<"\n"
                     <<"          --energy-error [Float]:  same as -e\n"
                     <<"          --eta-4th:     [Float]:  "<<eta_4th<<"\n"
                     <<"          --eta-2nd:     [Float]:  "<<eta_2nd<<"\n"
                     <<"          --eps:         [Float]:  "<<eps_sq<<"\n"
                     <<"    -G [Float]:  "<<G<<"\n"
                     <<"    -k [int]:    "<<sym_order<<"\n"
                     <<"          --load-par     [string]: "<<filename_par<<"\n"
                     <<"          --n-step-max   [int]  :  "<<nstep_max<<"\n"
                     <<"    -o [Float]:  "<<dt_output<<"\n"
                     <<"          --print-width     [int]: "<<print_width<<"\n"
                     <<"          --print-precision [int]: "<<print_precision<<"\n"
                     <<"    -p [string]: "<<filename_par<<"\n"
                     <<"    -r [Float]:  "<<r_break<<"\n"
                     <<"          --r-break      [Float]: same as -r\n"
                     <<"    -R [Float]:  "<<r_search<<"\n"
                     <<"          --slowdown-ref:           [Float]: "<<slowdown_ref<<"\n"
                     <<"          --slowdown-mass-ref       [Float]: "<<slowdown_mass_ref<<"\n"
                     <<"          --slowdown-timescale-max: [Float]: "<<slowdown_timescale_max<<"\n"
                     <<"    -t [Float]:  "<<time_end<<"\n"
                     <<"          --time-start   [Float]:  "<<time_zero<<"\n"
                     <<"          --time-end     [Float]:  same as -t\n"
                     <<"          --time-error   [Float]:  "<<time_error<<"\n"
                     <<"    -h :         print option information\n"
                     <<"          --help:                 same as -h\n";
            return 0;
        default:
            std::cerr<<"Unknown argument. check '-h' for help.\n";
            abort();
        }

    if (argc==1) {
        std::cerr<<"Please provide particle data filename\n";
        abort();
    }

    // data file name
    char* filename = argv[argc-1];

    // manager
    HermiteManager<HermiteInteraction> manager;
    AR::SymplecticManager<ARInteraction> ar_manager;

    Particle::r_break_crit = r_break.value;
    Particle::r_neighbor_crit = r_search.value;
    manager.step.eta_4th = eta_4th.value;
    manager.step.eta_2nd = eta_2nd.value;
    manager.step.setDtRange(dt_max.value, dt_min_power_index.value);
    manager.interaction.eps_sq = eps_sq.value;
    manager.interaction.G = G.value;
    ar_manager.interaction.eps_sq = eps_sq.value;
    ar_manager.interaction.G = G.value;
    ar_manager.time_step_real_min = manager.step.getDtMin();
    if (time_error.value == 0.0) ar_manager.time_error_max_real = 0.25*ar_manager.time_step_real_min;
    else ar_manager.time_error_max_real = time_error.value;

    ASSERT(ar_manager.time_error_max_real>1e-14);
    // time error cannot be smaller than round-off error
    ar_manager.energy_error_relative_max = energy_error.value; 
    ar_manager.slowdown_pert_ratio_ref = slowdown_ref.value;
    if (slowdown_timescale_max.value>0.0) ar_manager.slowdown_timescale_max = slowdown_timescale_max.value;
    else ar_manager.slowdown_timescale_max = time_end.value;
    ar_manager.step_count_max = nstep_max.value;
    // set symplectic order
    ar_manager.step.initialSymplecticCofficients(sym_order.value);

    // store input parameters
    std::string fpar_out = std::string(filename) + ".par";
    std::FILE* fout = std::fopen(fpar_out.c_str(),"w");
    if (fout==NULL) {
        std::cerr<<"Error: data file "<<fpar_out<<" cannot be open!\n";
        abort();
    }
    input_par_store.writeAscii(fout);
    fclose(fout);

    // integrator
    HermiteIntegrator<Particle, Particle, HermitePerturber, Neighbor<Particle>, HermiteInteraction, ARInteraction, HermiteInformation> h4_int;
    h4_int.manager = &manager;
    h4_int.ar_manager = &ar_manager;

    std::fstream fin;
    fin.open(filename,std::fstream::in);
    if(!fin.is_open()) {
        std::cerr<<"Error: data file "<<filename<<" cannot be open!\n";
        abort();
    }
    h4_int.particles.setMode(COMM::ListMode::local);
    h4_int.particles.readMemberAscii(fin);
    for (int i=0; i<h4_int.particles.getSize(); i++) h4_int.particles[i].id = i+1;
    h4_int.particles.calcCenterOfMass();
    h4_int.particles.shiftToCenterOfMassFrame();
    h4_int.particles.calcCenterOfMass();
        
    Float m_ave = h4_int.particles.cm.mass/h4_int.particles.getSize();
    manager.step.calcAcc0OffsetSq(m_ave, r_search.value);
    if (slowdown_mass_ref.value<=0.0) ar_manager.slowdown_mass_ref = m_ave;
    else ar_manager.slowdown_mass_ref = slowdown_mass_ref.value;

    // print parameters
    manager.print(std::cerr);
    ar_manager.print(std::cerr);


    std::cerr<<"CM: after shift ";
    h4_int.particles.cm.printColumn(std::cerr, 22);
    std::cerr<<std::endl;

    h4_int.groups.setMode(COMM::ListMode::local);
    h4_int.groups.reserveMem(h4_int.particles.getSize());
    h4_int.reserveIntegratorMem();
    // initial system 
    h4_int.initialSystemSingle(time_zero.value);
    h4_int.readGroupConfigureAscii(fin);

    // no initial when both parameters and data are load
    // initialization 
    h4_int.initialIntegration(); // get neighbors and min particles
    const int n_group_init = h4_int.getNGroup();
    h4_int.adjustGroups(true);
    h4_int.initialIntegration();
    h4_int.sortDtAndSelectActParticle();
    h4_int.info.time = h4_int.getTime();

    // precision
    std::cout<<std::setprecision(print_precision.value);

    // get initial energy
    h4_int.writeBackGroupMembers();
    h4_int.info.calcEnergy(h4_int.particles, manager.interaction, true);
    // cm
    h4_int.particles.calcCenterOfMass();
    std::cerr<<"CM:";
    h4_int.particles.cm.printColumn(std::cerr, 22);
    std::cerr<<std::endl;

    //print column title
    h4_int.info.printColumnTitle(std::cout, print_width.value);
    std::cout<<std::setw(print_width.value)<<"Ngroup";
    for (int i=0; i<n_group_init; i++) h4_int.groups[i].slowdown.printColumnTitle(std::cout, print_width.value);
    h4_int.particles.printColumnTitle(std::cout, print_width.value);
    std::cout<<std::endl;

    //print initial data
    h4_int.info.printColumn(std::cout, print_width.value);
    std::cout<<std::setw(print_width.value)<<n_group_init;
    for (int i=0; i<n_group_init; i++) h4_int.groups[i].slowdown.printColumn(std::cout, print_width.value);
    h4_int.particles.printColumn(std::cout, print_width.value);
    std::cout<<std::endl;
    
    // integration loop
    while (h4_int.info.time<time_end.value) {
        h4_int.integrateOneStepAct();
        h4_int.adjustGroups(false);
        h4_int.initialIntegration();
        h4_int.sortDtAndSelectActParticle();
        h4_int.info.time = h4_int.getTime();

        if (dt_output.value==0.0||fmod(h4_int.info.time, dt_output.value)==0.0) {
            h4_int.writeBackGroupMembers();
            h4_int.info.calcEnergy(h4_int.particles, manager.interaction, false);
            
            h4_int.particles.calcCenterOfMass();
            std::cerr<<"CM:";
            h4_int.particles.cm.printColumn(std::cerr, 22);
            std::cerr<<std::endl;

            h4_int.info.printColumn(std::cout, print_width.value);
            std::cout<<std::setw(print_width.value)<<n_group_init;
            for (int i=0; i<n_group_init; i++) h4_int.groups[i].slowdown.printColumn(std::cout, print_width.value);
            h4_int.particles.printColumn(std::cout, print_width.value);
            std::cout<<std::endl;
            h4_int.printStepHist();
        }
    }

    //fpu_fix_end(&oldcw);

    return 0;
}

