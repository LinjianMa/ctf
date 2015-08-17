/*Copyright (c) 2011, Edgar Solomonik, all rights reserved.*/

/** \addtogroup examples 
  * @{ 
  * \defgroup univar_accumulator_cust_sp univar_accumulator_cust_sp
  * @{ 
  * \brief tests cust_spom element-wise accumulator_cust_sps by implementing division elementwise on 4D tensors
  */

#include <ctf.hpp>
#include "moldynamics.h"
using namespace CTF;

int univar_accumulator_cust_sp(int     n,
                               World & dw){
  assert(n>1);
  
  Set<particle> sP = Set<particle>();
  Group<force> gF = Group<force>();
  
  Vector<particle> P(n, dw, sP);
  Matrix<force> F (true, n, n, AS, dw, gF);
  Matrix<force> F2(true, n, n, AS, dw, gF);

  particle * loc_parts;
  int64_t nloc;
  int64_t * inds;
  P.read_local(&nloc, &inds, &loc_parts);
  
  srand48(dw.rank);

  for (int64_t i=0; i<nloc; i++){
    //put first particle in middle, so its always within cutoff of all other particles
    if (inds[i] == 0){
      loc_parts[i].dx = .5;
      loc_parts[i].dy = .5;
    } else {
      loc_parts[i].dx = drand48();
      loc_parts[i].dy = drand48();
    }
    loc_parts[i].coeff = .001*drand48();
    loc_parts[i].id = 777;
  }
  P.write(nloc, inds, loc_parts);

  particle * all_parts;
  int64_t nall;
  std::vector< Pair<force> > my_forces;
  P.read_all(&nall, &all_parts);
  for (int i=0; i<nloc; i++){
    for (int j=0; j<nall; j++){
      if (j != inds[i]){
        //add force if distance within 1/sqrt(2)
        if (get_distance(loc_parts[i], all_parts[j])<.708){
          my_forces.push_back( Pair<force>(inds[i]*n+j, get_force(loc_parts[i], all_parts[j])));
        }
      }
    }
  }
  free(all_parts);
    
  F.write(my_forces.size(), &my_forces[0]);

  CTF::Univar_Accumulator<particle,force> uacc(&acc_force);

  //FIXME = does not work because it sets beta to addid :/
  F2["ij"] += F["ij"];
  F2["ij"] += F["ij"];

  //below is the same as uacc(F2["ij"],P["i"]);
  P["i"] += uacc(F2["ij"]);

  particle loc_parts_new[nloc];
  P.read(nloc, inds, loc_parts_new);
  
  //check that something changed
  int pass = 0;
  for (int64_t i=0; i<nloc; i++){
    if (fabs(loc_parts[i].dx - loc_parts_new[i].dx)>1.E-6 ||
        fabs(loc_parts[i].dy - loc_parts_new[i].dy)>1.E-6) pass = 1;
  }
  MPI_Allreduce(MPI_IN_PLACE, &pass, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
  if (!pass && dw.rank == 0){
    printf("Test incorrect: application of uacc did not modify some value.\n");
  }
  
  //FIXME = must invert tensors over groups via addinv() rather than - or -=, since these use the inverse of the multiplicative id
  F.addinv();
  uacc(F["ij"],P["i"]);
  uacc(F["ij"],P["i"]);

  P.read(nloc, inds, loc_parts_new);
  free(inds);
  
  if (pass){
    for (int64_t i=0; i<nloc; i++){
      if (fabs(loc_parts[i].dx - loc_parts_new[i].dx)>1.E-6 ||
          fabs(loc_parts[i].dy - loc_parts_new[i].dy)>1.E-6) pass = 0;
    }
  } 
  free(loc_parts);
  MPI_Allreduce(MPI_IN_PLACE, &pass, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

  if (dw.rank == 0){
    if (pass){
      printf("{ P[\"i\"] = uacc(F[\"ij\"]) } passed\n");
    } else {
      printf("{ P[\"i\"] = uacc(F[\"ij\"]) } failed\n");
    }
  } 

  
  return pass;
} 


#ifndef TEST_SUITE

char* getCmdOption(char ** begin,
                   char ** end,
                   const   std::string & option){
  char ** itr = std::find(begin, end, option);
  if (itr != end && ++itr != end){
    return *itr;
  }
  return 0;
}


int main(int argc, char ** argv){
  int rank, np, n;
  int const in_num = argc;
  char ** input_str = argv;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &np);

  if (getCmdOption(input_str, input_str+in_num, "-n")){
    n = atoi(getCmdOption(input_str, input_str+in_num, "-n"));
    if (n < 0) n = 5;
  } else n = 5;


  {
    World dw(MPI_COMM_WORLD, argc, argv);

    if (rank == 0){
      printf("Computing univar_accumulator_cust_sp A_ijkl = f(A_ijkl)\n");
    }
    univar_accumulator_cust_sp(n, dw);
  }


  MPI_Finalize();
  return 0;
}

/**
 * @} 
 * @}
 */

#endif
