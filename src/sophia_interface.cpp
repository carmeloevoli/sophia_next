#include "sophia_interface.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

/**
    This is a C++ version of SOPHIA.
    Translated by Mario Hoerbe (mario.hoerbe@ruhr-uni-bochum.de)
    2020
*/

/**
c*****************************************************************************
c**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!***
c**!!              IF YOU USE THIS PROGRAM, PLEASE CITE:                 !!***
c**!! A.M"ucke, Ralph Engel, J.P.Rachen, R.J.Protheroe and Todor Stanev, !!***
c**!!  1999, astro-ph/9903478, to appear in Comp.Phys.Commun.            !!***
c**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!***
c*****************************************************************************
c** Further SOPHIA related papers:                                         ***
c** (1) M"ucke A., et al 1999, astro-ph/9808279, to appear in PASA.        ***
c** (2) M"ucke A., et al 1999, to appear in: Proc. of the                  ***
c**      19th Texas Symposium on Relativistic Astrophysics, Paris, France, ***
c**      Dec. 1998. Eds.: J.~Paul, T.~Montmerle \& E.~Aubourg (CEA Saclay) ***
c** (3) M"ucke A., et al 1999, astro-ph/9905153, to appear in: Proc. of    ***
c**      19th Texas Symposium on Relativistic Astrophysics, Paris, France, ***
c**      Dec. 1998. Eds.: J.~Paul, T.~Montmerle \& E.~Aubourg (CEA Saclay) ***
c** (4) M"ucke A., et al 1999, to appear in: Proc. of 26th Int.Cosmic Ray  ***
c**      Conf. (Salt Lake City, Utah)                                      ***
c*****************************************************************************
*/

void sophia_interface::debug(std::string errm, bool stopProgram) {
  std::cout << "------------------------------------" << std::endl;
  std::cout << "debug: " << errm << std::endl;
  std::cout << "N = " << N << std::endl;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < 5; ++j) {
      std::cout << K[j][i] << "\tK|P\t" << P[j][i] << std::endl;
    }
  }
  std::cout << "test random number: " << RNDM() << std::endl;
  std::cout << "------------------------------------" << std::endl;
  if (stopProgram) throw std::runtime_error("stopped by debug.");
}

void sophia_interface::debugNonLUND(std::string errm, bool stopProgram) {
  std::cout << "------------------------------------" << std::endl;
  std::cout << "debugNonLUND: " << errm << std::endl;
  std::cout << "np = " << np << std::endl;
  for (int i = 0; i < np; ++i) {
    std::cout << "ID = " << ID_sophia_to_PDG(LLIST[i]) << std::endl;
    for (int j = 0; j < 5; ++j) {
      std::cout << "|P\t" << p[j][i] << std::endl;
    }
  }
  std::cout << "test random number: " << RNDM() << std::endl;
  std::cout << "------------------------------------" << std::endl;
  if (stopProgram) throw std::runtime_error("stopped by debugNonLUND.");
}

sophiaevent_output sophia_interface::sophiaevent(bool onProton, double Ein, double eps,
                                                 bool declareChargedPionsStable) {
  // ****************************************************************************
  //    SOPHIAEVENT
  //
  //    interface between Sophia and CRPropa
  //    simulate an interaction between p/n of given energy and a photon
  //
  //    Eric Armengaud, 2005
  //    modified & translated from FORTRAN to C++: Mario Hoerbe, 2020
  // *******************************
  //  onProton = primary particle is proton or neutron
  //  Ein = input energy of primary nucleon in GeV (SOPHIA standard energy unit)
  //  eps = input energy of target photon in GeV (SOPHIA standard energy unit)
  //  declareChargedPionsStable = pi+-0 are set to be stable particles. See array IDB for details
  //  OutPartP = list of 4-momenta + rest masses of output particles (neutrinos approx. 0)
  //  OutPartID = ID list of output particles (PDG IDs)
  //  Nout = number of output particles
  // ****************************************************************************
  const double pi = 3.141592653;

  if (declareChargedPionsStable) {
    IDB[5] = 0;  // pi0 = stable
    IDB[6] = 0;  // pi+ = stable
    IDB[7] = 0;  // pi- = stable
  }

  int L0 = onProton ? 13 : 14;
  double E0 = Ein;
  double pm = AM[L0 - 1];
  double s = sample_s(eps, L0, E0);
  double Pp = std::sqrt(E0 * E0 - pm * pm);
  double theta = ((pm * pm - s) / 2. / eps + E0) / Pp;
  if (theta > 1.) {
    std::cout << "sophiaevent: theta > 1.: " << theta << std::endl;
    theta = 0.;
  } else if (theta < -1.) {
    std::cout << "sophiaevent: theta < -1.: " << theta << std::endl;
    theta = 180.;
  } else {
    theta = std::acos(theta) * 180. / pi;
  }

  eventgen(L0, E0, eps, theta);

  // generate output
  sophiaevent_output seo;
  for (int i = 0; i < np; ++i) {
    for (int j = 0; j < 5; ++j) {
      seo.outPartP[j][i] = p[j][i];
    }
    seo.outPartID[i] = LLIST[i];
  }
  seo.Nout = np;
  return seo;
}

void sophia_interface::eventgen(int L0, double E0, double eps, double theta) {
  // *******************************************************
  // ** subroutine for photopion production of            **
  // ** relativistic nucleons in a soft photon field      **
  // ** subroutine for SOPHIA inVersion 1.2               **
  // ****** INPUT ******************************************
  //  E0 = energy of incident proton (in lab frame) [in GeV]
  //  eps = energy of incident photon [in GeV] (in lab frame)
  //  theta = angle between incident proton and photon [in degrees]
  //  L0 = code number of the incident nucleon
  // ****** OUTPUT *************************************************
  //  P(2000,5) = 5-momentum of produced particles
  //  LLIST(2000) = code numbers of produced particles
  //  NP = number of produced particles
  // ***************************************************************
  // ** Date: 20/01/98       **
  // ** correct.:19/02/98    **
  // ** change:  23/05/98    **
  // ** last change:06/09/98 **
  // ** authors: A.Muecke    **
  // **          R.Engel     **
  // **************************
  const int IRESMAX = 9;
  const double pi = 3.1415926;

  double P_nuc[4] = {0.};
  double P_gam[4] = {0.};
  double P_sum[4] = {0.};
  double PC[4] = {0.};
  double GamBet[4] = {0.};
  int Icount = 0;

  // incoming nucleon
  double pm = AM[L0 - 1];
  P_nuc[0] = 0.;
  P_nuc[1] = 0.;
  P_nuc[2] = std::sqrt(std::max((E0 - pm) * (E0 + pm), 0.));
  P_nuc[3] = E0;

  // incoming photon
  P_gam[0] = eps * std::sin(theta * pi / 180.);
  P_gam[1] = 0.;
  P_gam[2] = -eps * std::cos(theta * pi / 180.);
  P_gam[3] = eps;

  double Esum = P_nuc[3] + P_gam[3];
  double PXsum = P_nuc[0] + P_gam[0];
  double PYsum = P_nuc[1] + P_gam[1];
  double PZsum = P_nuc[2] + P_gam[2];
  int IQchr = ICHP[0] + ICHP[L0 - 1];
  int IQbar = IBAR[0] + IBAR[L0 - 1];

  double gammap = E0 / pm;
  double xx = 1. / gammap;
  double betap = 0.;
  if (gammap > 1000.) {
    betap = 1. - 0.5 * xx * xx - 0.125 * xx * xx * xx * xx;
  } else {
    betap = std::sqrt(1. - xx) * std::sqrt(1. + xx);
  }

  double s = pm * pm + 2. * eps * E0 * (1. - betap * std::cos(theta * pi / 180.));
  double sqsm = std::sqrt(s);
  double eps_prime = (s - pm * pm) / 2. / pm;

  // calculate Lorentz boost and rotation
  P_sum[0] = P_nuc[0] + P_gam[0];
  P_sum[1] = P_nuc[1] + P_gam[1];
  P_sum[2] = P_nuc[2] + P_gam[2];
  P_sum[3] = P_nuc[3] + P_gam[3];

  // C  Lorentz transformation into c.m. system
  for (int I = 0; I < 4; ++I) {
    GamBet[I] = P_sum[I] / sqsm;
  }

  // calculate rotation angles
  double COD = 1.;
  double SID = 0.;
  double COF = 1.;
  double SIF = 0.;
  if (GamBet[3] < 1.e5) {
    // transform nucleon vector
    GamBet[0] = -GamBet[0];
    GamBet[1] = -GamBet[1];
    GamBet[2] = -GamBet[2];

    PO_ALTRA_output pao = PO_ALTRA(GamBet[3], GamBet[0], GamBet[1], GamBet[2], P_nuc[0], P_nuc[1],
                                   P_nuc[2], P_nuc[3]);
    double Ptot = pao.P;
    PC[0] = pao.PX;
    PC[1] = pao.PY;
    PC[2] = pao.PZ;
    PC[3] = pao.E;
    GamBet[0] = -GamBet[0];
    GamBet[1] = -GamBet[1];
    GamBet[2] = -GamBet[2];

    // rotation angle: nucleon moves along +z
    COD = PC[2] / Ptot;
    SID = std::sqrt(PC[0] * PC[0] + PC[1] * PC[1]) / Ptot;
    if (Ptot * SID > 1.e-5) {
      COF = PC[0] / (SID * Ptot);
      SIF = PC[1] / (SID * Ptot);
      double Anorf = std::sqrt(COF * COF + SIF * SIF);
      COF /= Anorf;
      SIF /= Anorf;
    }
  }

  // check for threshold:
  const double sth = 1.1646;
  if (s < sth) {
    std::cout << "input energy below threshold for photopion production! sqrt(s) = " << std::sqrt(s)
              << std::endl;
    np = 0;
    return;
  }

  // 200
  Icount++;

  // ********************************************************************
  // c decide which process occurs:                                   ***
  // c (1) decay of resonance                                         ***
  // c (2) direct pion production (interaction of photon with         ***
  // c     virtual pions in nucleon cloud) and diffractive scattering ***
  // c (3) multipion production                                       ***
  // ********************************************************************

  int Imode = dec_inter3(eps_prime, L0);
  // ******* PARTICLE PRODUCTION *****************
  if (Imode <= 5) {
    // direct/multipion/diffractive scattering production channel:
    gamma_h(sqsm, L0, Imode);

  } else if (Imode == 6) {
    // Resonances:
    // decide which resonance decays with ID=IRES in list:
    // IRESMAX = number of considered resonances = 9 so far
    int IRES = 0;

    // 46
    IRES = dec_res2(eps_prime, IRESMAX, L0);
    int Nproc = 10 + IRES;
    dec_proc2_output dpo = dec_proc2(eps_prime, IRES, L0);
    int IPROC = dpo.IPROC;
    int IRANGE = dpo.IRANGE;

    // 2-particle decay of resonance in CM system:
    np = 2;
    RES_DECAY3(IRES, IPROC, IRANGE, s, L0);
    DECSIB();
  } else {
    throw std::runtime_error("eventgen: invalid Imode");
  }

  // consider only stable particles:
  int istable = 0;
  for (int i = 0; i < np; ++i) {
    if (std::abs(LLIST[i]) < 10000) {
      istable++;
      LLIST[istable - 1] = LLIST[i];
      p[0][istable - 1] = p[0][i];
      p[1][istable - 1] = p[1][i];
      p[2][istable - 1] = p[2][i];
      p[3][istable - 1] = p[3][i];
      p[4][istable - 1] = p[4][i];
    }
  }
  if (np > istable) {
    for (int i = istable; i < np; ++i) {
      LLIST[i] = 0;
      p[0][i] = 0.;
      p[1][i] = 0.;
      p[2][i] = 0.;
      p[3][i] = 0.;
      p[4][i] = 0.;
    }
  }
  np = istable;

  // transformation from CM-system to lab-system:
  for (int I = 0; I < np; ++I) {
    std::vector<double> PO_TRANS_output = PO_TRANS(p[0][I], p[1][I], p[2][I], COD, SID, COF, SIF);
    PC[0] = PO_TRANS_output[0];
    PC[1] = PO_TRANS_output[1];
    PC[2] = PO_TRANS_output[2];
    PC[3] = p[3][I];
    PO_ALTRA_output poa =
        PO_ALTRA(GamBet[3], GamBet[0], GamBet[1], GamBet[2], PC[0], PC[1], PC[2], PC[3]);
    p[0][I] = poa.PX;
    p[1][I] = poa.PY;
    p[2][I] = poa.PZ;
    p[3][I] = poa.E;
  }

  return;
}

static int Ic = 0;
void sophia_interface::gamma_h(double Ecm, int ip1, int Imode) {
  // **********************************************************************
  //
  //      simple simulation of low-energy interactions (R.E. 03/98)
  //
  //      changed to simulate superposition of reggeon and pomeron exchange
  //      interface to Lund / JETSET 7.4 fragmentation
  //                                                   (R.E. 08/98)
  //
  //      input: ip1    incoming particle
  //                    13 - p
  //                    14 - n
  //             Ecm    CM energy in GeV
  //             Imode  interaction mode
  //                    0 - multi-pion fragmentation
  //                    5 - fragmentation in resonance region
  //                    1 - quasi-elastic / diffractive interaction
  //                        (p/n-gamma  --> n/p rho)
  //                    4 - quasi-elastic / diffractive interaction
  //                        (p/n-gamma  --> n/p omega)
  //                    2 - direct interaction (p/n-gamma  --> n/p pi)
  //                    3 - direct interaction (p/n-gamma  --> delta pi)
  //             IFBAD control flag
  //                   (0  all OK,
  //                    1  generation of interaction not possible)
  //
  // **********************************************************************
  double P_dec[5][10] = {0.};
  double P_in[5] = {0.};
  double xs1[2] = {0.};
  double xs2[2] = {0.};
  double xmi[2] = {0.};
  double xma[2] = {0.};
  int LL[10] = {0};
  int Ijoin[4] = {0};

  double PA1[4] = {0.};
  double PA2[4] = {0.};
  double P1[4] = {0.};
  double P2[4] = {0.};

  // parameters of pi0 suppression
  double a1 = 0.5;
  double a2 = 0.5;

  // second particle is always photon
  const int ip2 = 1;

  // slope of pomeron trajectory
  double alphap = 0.25;

  // further "globals"
  int Nproc = 0;
  double SQS = Ecm;
  double S = SQS * SQS;
  Ic++;

  if (Imode == 1 || Imode == 4) {
    // simulation of diffraction
    int ipa = ip1;
    int ipb = ip2;
    if (Imode == 1) {
      Nproc = 1;
      if (ip1 == 1) {
        ipa = 27;
      } else if (ip2 == 1) {
        ipb = 27;
      }
    } else if (Imode == 4) {
      Nproc = 4;
      if (ip1 == 1) {
        ipa = 32;
      } else if (ip2 == 1) {
        ipb = 32;
      }
    }

    double am_a = AM[ipa - 1];
    double am_b = AM[ipb - 1];
    if (am_a + am_b >= Ecm - 1.e-2) am_a = Ecm - am_b - 1.e-2;

    // find t range
    double e1 = 0.5 * (Ecm + AM[ip1 - 1] * AM[ip1 - 1] / Ecm - AM[ip2 - 1] * AM[ip2 - 1] / Ecm);
    double pcm1 = 0;
    if (e1 > 100. * AM[ip1 - 1]) {
      pcm1 = e1 - 0.5 * AM[ip1 - 1] * AM[ip1 - 1] / e1;
    } else {
      pcm1 = std::sqrt((e1 - AM[ip1 - 1]) * (e1 + AM[ip1 - 1]));
    }
    double e3 = 0.5 * (Ecm + am_a * am_a / Ecm - am_b * am_b / Ecm);
    double pcm3 = 0.;
    if (e3 > 100. * am_a) {
      pcm3 = e3 - 0.5 * am_a * am_a / e3;
    } else {
      pcm3 = std::sqrt((e3 - am_a) * (e3 + am_a));
    }
    double t0 = std::pow((AM[ip1 - 1] * AM[ip1 - 1] - am_a * am_a - AM[ip2 - 1] * AM[ip2 - 1] +
                          am_b * am_b) /
                             (2. * Ecm),
                         2) -
                (pcm1 - pcm3) * (pcm1 - pcm3) - 0.0001;
    double t1 = std::pow((AM[ip1 - 1] * AM[ip1 - 1] - am_a * am_a - AM[ip2 - 1] * AM[ip2 - 1] +
                          am_b * am_b) /
                             (2. * Ecm),
                         2) -
                (pcm1 + pcm3) * (pcm1 + pcm3) + 0.0001;

    // sample t
    double b = 6.5 + 2. * alphap * std::log(S);
    double t = 1. / b * std::log((std::exp(b * t0) - std::exp(b * t1)) * RNDM() + std::exp(b * t1));

    // kinematics
    double pl = (2. * e1 * e3 + t - AM[ip1 - 1] * AM[ip1 - 1] - am_a * am_a) / (2. * pcm1);
    double pt = (pcm3 - pl) * (pcm3 + pl);
    if (pt < 0.) {
      int sgn = (pl < 0) ? -1 : 1;
      pl = pcm3 * sgn;
      pt = 1.e-6;
    } else {
      pt = std::sqrt(pt);
    }
    double phi = 6.28318530717959 * RNDM();

    LLIST[0] = ipa;
    p[3][0] = e3;
    p[0][0] = std::sin(phi) * pt;
    p[1][0] = std::cos(phi) * pt;
    p[2][0] = pl;
    p[4][0] = am_a;
    LLIST[1] = ipb;
    p[0][1] = -p[0][0];
    p[1][1] = -p[1][0];
    p[2][1] = -p[2][0];
    p[3][1] = Ecm - p[3][0];
    p[4][1] = am_b;
    np = 2;

    DECSIB();

  } else if (Imode == 2 || Imode == 3) {
    int ipa = 0.;
    int ipb = 0.;

    // simulation of direct p-gamma process
    if (ip1 == 13) {
      // projectile is a proton
      if (Imode == 2) {
        Nproc = 2;
        ipa = 14;
        ipb = 7;
      } else {
        Nproc = 3;
        if (RNDM() > 0.25) {
          ipa = 40;
          ipb = 8;
        } else {
          ipa = 42;
          ipb = 7;
        }
      }
    } else if (ip1 == 14) {
      // projectile is a neutron
      if (Imode == 2) {
        Nproc = 2;
        ipa = 13;
        ipb = 8;
      } else {
        Nproc = 3;
        if (RNDM() > 0.25) {
          ipa = 43;
          ipb = 7;
        } else {
          ipa = 41;
          ipb = 8;
        }
      }
    }

    double am_a = AM[ipa - 1];
    double am_b = AM[ipb - 1];
    if (am_a + am_b >= Ecm - 1.e-3) am_a = Ecm - am_b - 1.e-3;

    // find t range
    double e1 = 0.5 * (Ecm + AM[ip1 - 1] * AM[ip1 - 1] / Ecm - AM[ip2 - 1] * AM[ip2 - 1] / Ecm);
    double pcm1 = 0.;
    if (e1 > 100. * AM[ip1 - 1]) {
      pcm1 = e1 - 0.5 * AM[ip1 - 1] * AM[ip1 - 1] / e1;
    } else {
      pcm1 = std::sqrt((e1 - AM[ip1 - 1]) * (e1 + AM[ip1 - 1]));
    }
    double e3 = 0.5 * (Ecm + am_a * am_a / Ecm - am_b * am_b / Ecm);
    double pcm3 = 0.;
    if (e3 > 100. * am_a) {
      pcm3 = e3 - 0.5 * am_a * am_a / e3;
    } else {
      pcm3 = std::sqrt((e3 - am_a) * (e3 + am_a));
    }
    double t0 = std::pow((AM[ip1 - 1] * AM[ip1 - 1] - am_a * am_a - AM[ip2 - 1] * AM[ip2 - 1] +
                          am_b * am_b) /
                             (2. * Ecm),
                         2) -
                (pcm1 - pcm3) * (pcm1 - pcm3) - 0.0001;
    double t1 = std::pow((AM[ip1 - 1] * AM[ip1 - 1] - am_a * am_a - AM[ip2 - 1] * AM[ip2 - 1] +
                          am_b * am_b) /
                             (2. * Ecm),
                         2) -
                (pcm1 + pcm3) * (pcm1 + pcm3) + 0.0001;

    // sample t
    double b = 12.;
    double t = 1. / b * std::log((std::exp(b * t0) - exp(b * t1)) * RNDM() + std::exp(b * t1));

    // kinematics
    double pl = (2. * e1 * e3 + t - AM[ip1 - 1] * AM[ip1 - 1] - am_a * am_a) / (2. * pcm1);
    double pt = (pcm3 - pl) * (pcm3 + pl);
    if (pt < 0.) {
      int sgn = (pl < 0) ? -1 : 1;
      pl = pcm3 * sgn;
      pt = 1.e-6;
    } else {
      pt = std::sqrt(pt);
    }
    double phi = 6.28318530717959 * RNDM();

    LLIST[0] = ipa;
    p[3][0] = e3;
    p[0][0] = std::sin(phi) * pt;
    p[1][0] = std::cos(phi) * pt;
    p[2][0] = pl;
    p[4][0] = am_a;
    LLIST[1] = ipb;
    p[0][1] = -p[0][0];
    p[1][1] = -p[1][0];
    p[2][1] = -p[2][0];
    p[3][1] = Ecm - p[3][0];
    p[4][1] = am_b;
    np = 2;

    DECSIB();

  } else {
    // simulation of multiparticle production via fragmentation
    Nproc = 0;

    double SIG_reg = 129. * std::pow(S - AM[12] * AM[12], -0.4525);
    double SIG_pom = 67.7 * std::pow(S - AM[12] * AM[12], 0.0808);
    double prob_reg = (S > 2.6) ? SIG_reg / (SIG_pom + SIG_reg) : 1.;
    double ptu = 0.36 + 0.08 * std::log10(SQS / 30.);
    double s1 = 1.2;
    double s2 = 0.6;
    double as1 = s1 * s1 / S;
    double as2 = s2 * s2 / S;
    if (s1 + s2 >= SQS - 0.2) prob_reg = 1.;

    int ima_0 = 0;
    int imb_0 = 0;
    int ima_1 = 0;
    int imb_1 = 0;
    int ima_2 = 0;
    int imb_2 = 0;
    int ima_3 = 0;
    int imb_3 = 0;
    int iba_0 = 0;
    int ibb_0 = 0;
    int iba_1 = 0;
    int ibb_1 = 0;
    int iba_2 = 0;
    int ibb_2 = 0;
    int iba_3 = 0;
    int ibb_3 = 0;

    int imul = 0;
    int itry = 0;
    int Ifl1a = 0;
    int Ifl1b = 0;
    int Ifl2a = 0;
    int Ifl2b = 0;

    bool repeat100 = false;
    do {  // 100
      repeat100 = false;
      int Istring = 0;

      // avoid infinite looping
      itry++;
      if (itry > 50) {
        throw std::runtime_error("gamma_h: more than 50 internal rejections");
      }

      // simulate reggeon (one-string topology)
      if (RNDM() < prob_reg) {
        for (int i = 0; i < 1000; ++i) {
          std::vector<int> valences_output1 = valences(ip1);
          Ifl1a = valences_output1[0];
          Ifl1b = valences_output1[1];
          std::vector<int> valences_output2 = valences(ip2);
          Ifl2a = valences_output2[0];
          Ifl2b = valences_output2[1];
          if (Ifl1b == -Ifl2b) break;
          if (i == 999) {
            std::cout << "gamma_h: simulation of reggeon impossible:" << ip1 << ", " << ip2
                      << std::endl;
            repeat100 = true;
            break;
          }
        }
        if (repeat100) continue;

        // 200
        np = 0;
        Istring = 1;
        double ee = Ecm / 2.;
        double pt = 0.;
        do {
          pt = ptu * std::sqrt(-std::log(std::max(1.e-10, RNDM())));
        } while (pt >= ee);

        double phi = 6.2831853 * RNDM();
        double px = pt * std::cos(phi);
        double py = pt * std::sin(phi);
        double pz = std::sqrt(ee * ee - px * px - py * py);
        lund_put(1, Ifl1a, px, py, pz, ee);
        px = -px;
        py = -py;
        pz = -pz;
        lund_put(2, Ifl2a, px, py, pz, ee);
        Ijoin[0] = 1;
        Ijoin[1] = 2;

        LUJOIN(2, Ijoin);

        lund_frag(Ecm);

        if (np < 0) {
          np = 0;
          {
            repeat100 = true;
            continue;
          }
        }
        for (int i = 0; i < np; ++i) {
          lund_get_output lgo = lund_get(i + 1);
          p[0][i] = lgo.PX;
          p[1][i] = lgo.PY;
          p[2][i] = lgo.PZ;
          p[3][i] = lgo.EE;
          p[4][i] = lgo.XM;
          LLIST[i] = lgo.IFL;
        }

        // simulate pomeron (two-string topology)
      } else {
        std::vector<int> valences_output1 = valences(ip1);
        int Ifl1a = valences_output1[0];
        int Ifl1b = valences_output1[1];
        std::vector<int> valences_output2 = valences(ip2);
        int Ifl2a = valences_output2[0];
        int Ifl2b = valences_output2[1];
        if (Ifl1a * Ifl2a < 0) {
          int j = Ifl2a;
          Ifl2a = Ifl2b;
          Ifl2b = j;
        }
        double pl1 = (1. + as1 - as2);
        double ps1 = 0.25 * pl1 * pl1 - as1;
        if (ps1 <= 0.) {
          prob_reg = 1.;
          {
            repeat100 = true;
            continue;
          }
        }
        ps1 = std::sqrt(ps1);
        xmi[0] = 0.5 * pl1 - ps1;
        xma[0] = 0.5 * pl1 + ps1;

        double pl2 = 1. + as2 - as1;
        double ps2 = 0.25 * pl2 * pl2 - as2;
        if (ps2 <= 0.) {
          prob_reg = 1.;
          {
            repeat100 = true;
            continue;
          }
        }
        ps2 = std::sqrt(ps2);
        xmi[1] = 0.5 * pl2 - ps2;
        xma[1] = 0.5 * pl2 + ps2;

        if (xmi[0] >= xma[0] + 0.05 || xmi[1] >= xma[1] + 0.05) {
          prob_reg = 1.;
          {
            repeat100 = true;
            continue;
          }
        }

        PO_SELSX2_output pso = PO_SELSX2(xmi, xma, as1, as2);
        xs1[0] = pso.XS1[0];
        xs1[1] = pso.XS1[1];
        xs2[0] = pso.XS2[0];
        xs2[1] = pso.XS2[1];
        bool isRejected = pso.isRejected;
        if (isRejected) {
          prob_reg = 1.;
          {
            repeat100 = true;
            continue;
          }
        }

        np = 0;
        Istring = 1;

        double ee = std::sqrt(xs1[0] * xs2[0]) * Ecm / 2.;
        double pt = 0.;
        do {
          pt = ptu * std::sqrt(-std::log(std::max(1.e-10, RNDM())));
        } while (pt >= ee);
        double phi = 6.2831853 * RNDM();
        double px = pt * std::cos(phi);
        double py = pt * std::sin(phi);
        double pz = 0.;

        PA1[0] = px;
        PA1[1] = py;
        PA1[2] = xs1[0] * Ecm / 2.;
        PA1[3] = PA1[2];

        PA2[0] = -px;
        PA2[1] = -py;
        PA2[2] = -xs2[0] * Ecm / 2.;
        PA2[3] = -PA2[2];

        double XM1 = 0.;
        double XM2 = 0.;
        PO_MSHELL_output pmo = PO_MSHELL(PA1, PA2, XM1, XM2);
        px = pmo.P1[0];
        py = pmo.P1[1];
        pz = pmo.P1[2];
        ee = pmo.P1[3];
        lund_put(1, Ifl1a, px, py, pz, ee);
        px = pmo.P2[0];
        py = pmo.P2[1];
        pz = pmo.P2[2];
        ee = pmo.P2[3];
        lund_put(2, Ifl2a, px, py, pz, ee);

        Ijoin[0] = 1;
        Ijoin[1] = 2;
        LUJOIN(2, Ijoin);

        ee = std::sqrt(xs1[1] * xs2[1]) * Ecm / 2.;

        do {
          pt = ptu * std::sqrt(-std::log(std::max(1.e-10, RNDM())));
        } while (pt >= ee);
        phi = 6.2831853 * RNDM();
        px = pt * std::cos(phi);
        py = pt * std::sin(phi);

        PA1[0] = px;
        PA1[1] = py;
        PA1[2] = xs1[1] * Ecm / 2.;
        PA1[3] = PA1[2];

        PA2[0] = -px;
        PA2[1] = -py;
        PA2[2] = -xs2[1] * Ecm / 2.;
        PA2[3] = -PA2[2];

        XM1 = 0.;
        XM2 = 0.;
        pmo = PO_MSHELL(PA1, PA2, XM1, XM2);
        px = pmo.P1[0];
        py = pmo.P1[1];
        pz = pmo.P1[2];
        ee = pmo.P1[3];
        lund_put(3, Ifl1b, px, py, pz, ee);
        px = pmo.P2[0];
        py = pmo.P2[1];
        pz = pmo.P2[2];
        ee = pmo.P2[3];
        lund_put(4, Ifl2b, px, py, pz, ee);

        Ijoin[0] = 3;
        Ijoin[1] = 4;
        LUJOIN(2, Ijoin);

        lund_frag(Ecm);
        if (np < 0) {
          np = 0;
          prob_reg += 0.1;
          {
            repeat100 = true;
            continue;
          }
        }

        for (int i = 0; i < np; ++i) {
          lund_get_output lgo = lund_get(i + 1);
          p[0][i] = lgo.PX;
          p[1][i] = lgo.PY;
          p[2][i] = lgo.PZ;
          p[3][i] = lgo.EE;
          p[4][i] = lgo.XM;
          LLIST[i] = lgo.IFL;
        }
      }

      // for fragmentation in resonance region:
      if (Imode == 5) {
        DECSIB();
        int IQchr = ICHP[ip1 - 1] + ICHP[ip2 - 1];
        int IQbar = IBAR[ip1 - 1] + IBAR[ip2 - 1];
        check_event(-Ic, Ecm, 0., 0., 0., IQchr, IQbar);
        return;
      }

      // leading baryon/meson effect
      for (int j = 0; j < np; ++j) {
        if ((LLIST[j] == 13 || LLIST[j] == 14) && p[2][j] < 0.) {
          if (RNDM() < std::pow(2. * p[3][j] / Ecm, 2)) {
            repeat100 = true;
            break;
          }
        }
        if (LLIST[j] >= 6 && LLIST[j] <= 8 && p[2][j] < -0.4) {
          if (RNDM() < std::pow(2. * p[3][j] / Ecm, 2)) {
            repeat100 = true;
            break;
          }
        }
      }
      if (repeat100) continue;

      ima_0 = 0;
      imb_0 = 0;
      imb_1 = 0;
      ima_1 = 0;
      imb_2 = 0;
      ima_2 = 0;
      imul = 0;

      // remove elastic/diffractive channels
      if (ip1 == 1) {
        iba_0 = 6;
        iba_1 = 27;
        iba_2 = 32;
      } else {
        iba_0 = ip1;
        iba_1 = ip1;
        iba_2 = ip1;
      }
      if (ip2 == 1) {
        ibb_0 = 6;
        ibb_1 = 27;
        ibb_2 = 32;
      } else {
        ibb_0 = ip2;
        ibb_1 = ip2;
        ibb_2 = ip2;
      }

      for (int j = 0; j < np; ++j) {
        int l1 = std::abs(LLIST[j]);
        if (l1 < 10000) {
          if (LLIST[j] == iba_0) ima_0 = 1;
          if (LLIST[j] == iba_1) ima_1 = 1;
          if (LLIST[j] == iba_2) ima_2 = 1;
          if (LLIST[j] == ibb_0) imb_0 = 1;
          if (LLIST[j] == ibb_1) imb_1 = 1;
          if (LLIST[j] == ibb_2) imb_2 = 1;
          imul++;
        }
      }

      if (imul == 2) {
        if (ima_0 * imb_0 == 1) {
          repeat100 = true;
          continue;
        }
        if (ima_1 * imb_1 == 1) {
          repeat100 = true;
          continue;
        }
        if (ima_2 * imb_2 == 1) {
          repeat100 = true;
          continue;
        }
      }

      // remove direct channels
      if (imul == 2 && ip2 == 1 && (ip1 == 13 || ip1 == 14)) {
        ima_0 = 0;
        imb_0 = 0;
        ima_1 = 0;
        imb_1 = 0;
        ima_2 = 0;
        imb_2 = 0;
        ima_3 = 0;
        imb_3 = 0;

        if (ip1 == 13) {
          iba_0 = 14;
          ibb_0 = 7;
          iba_1 = 40;
          ibb_1 = 8;
          iba_2 = 42;
          ibb_2 = 7;
          iba_3 = 13;
          ibb_3 = 23;
        } else {
          iba_0 = 13;
          ibb_0 = 8;
          iba_1 = 43;
          ibb_1 = 7;
          iba_2 = 41;
          ibb_2 = 8;
          iba_3 = 14;
          ibb_3 = 23;
        }
        for (int j = 0; j < np; ++j) {
          int l1 = std::abs(LLIST[j]);
          if (l1 < 10000) {
            if (LLIST[j] == iba_0) ima_0 = 1;
            if (LLIST[j] == iba_1) ima_1 = 1;
            if (LLIST[j] == iba_2) ima_2 = 1;
            if (LLIST[j] == iba_3) ima_3 = 1;
            if (LLIST[j] == ibb_0) imb_0 = 1;
            if (LLIST[j] == ibb_1) imb_1 = 1;
            if (LLIST[j] == ibb_2) imb_2 = 1;
            if (LLIST[j] == ibb_3) imb_3 = 1;
          }
        }
        if (ima_0 * imb_0 == 1) {
          repeat100 = true;
          continue;
        }
        if (ima_1 * imb_1 == 1) {
          repeat100 = true;
          continue;
        }
        if (ima_2 * imb_2 == 1) {
          repeat100 = true;
          continue;
        }
        if (ima_3 * imb_3 == 1) {
          repeat100 = true;
          continue;
        }
      }

      // suppress events with many pi0's
      ima_0 = 0;
      imb_0 = 0;
      for (int j = 0; j < np; ++j) {
        // neutral mesons
        if (LLIST[j] == 6) ima_0++;
        if (LLIST[j] == 11) ima_0++;
        if (LLIST[j] == 12) ima_0++;
        if (LLIST[j] == 21) ima_0++;
        if (LLIST[j] == 22) ima_0++;
        if (LLIST[j] == 23) ima_0++;
        if (LLIST[j] == 24) ima_0++;
        if (LLIST[j] == 27) ima_0++;
        if (LLIST[j] == 32) ima_0++;
        if (LLIST[j] == 33) ima_0++;
        // charged mesons
        if (LLIST[j] == 7) imb_0++;
        if (LLIST[j] == 8) imb_0++;
        if (LLIST[j] == 9) imb_0++;
        if (LLIST[j] == 10) imb_0++;
        if (LLIST[j] == 25) imb_0++;
        if (LLIST[j] == 26) imb_0++;
      }

      double prob_1 = a1 * imb_0 / std::max(ima_0 + imb_0, 1) + a2;
      if (RNDM() > prob_1) {
        repeat100 = true;
        continue;
      }
    } while (repeat100);

    // correct multiplicity at very low energies
    int ND = 0;
    double E_ref_1 = 1.6;
    double E_ref_2 = 1.95;

    if (imul == 3 && Ecm > E_ref_1 && Ecm < E_ref_2) {
      ima_0 = 0;
      ima_1 = 0;
      ima_2 = 0;
      imb_0 = 0;
      imb_1 = 0;
      iba_0 = 0;
      iba_1 = 0;
      iba_2 = 0;
      ibb_0 = 0;
      ibb_1 = 0;

      // incoming proton
      if (ip1 == 13) {
        iba_0 = 13;
        iba_1 = 7;
        iba_2 = 8;
        ibb_0 = 14;
        ibb_1 = 6;

        // incoming neutron
      } else if (ip1 == 14) {
        iba_0 = 14;
        iba_1 = 7;
        iba_2 = 8;
        ibb_0 = 13;
        ibb_1 = 6;
      }

      for (int j = 0; j < np; ++j) {
        if (LLIST[j] == iba_0) ima_0++;
        if (LLIST[j] == iba_1) ima_1++;
        if (LLIST[j] == iba_2) ima_2++;
        if (LLIST[j] == ibb_0) imb_0++;
        if (LLIST[j] == ibb_1) imb_1++;
      }

      // N gamma --> N pi +  pi-
      if (ima_0 * ima_1 * ima_2 == 1) {
        double Elog = std::log(Ecm);
        double Elog_1 = std::log(E_ref_1);
        double Elog_2 = std::log(E_ref_2);
        double prob = 0.1 * 4. / std::pow(Elog_2 - Elog_1, 2) * (Elog - Elog_1) * (Elog_2 - Elog);
        if (RNDM() < prob) {
          LL[0] = ip1;
          LL[1] = 7;
          LL[2] = 8;
          LL[3] = 6;
          ND = 4;
        }
      }
    }

    E_ref_1 = 1.95;
    E_ref_2 = 2.55;

    if (imul == 4 && Ecm > E_ref_1 && Ecm < E_ref_2) {
      ima_0 = 0;
      ima_1 = 0;
      ima_2 = 0;
      imb_0 = 0;
      imb_1 = 0;
      iba_0 = 0;
      iba_1 = 0;
      iba_2 = 0;
      ibb_0 = 0;
      ibb_1 = 0;

      // incoming proton
      if (ip1 == 13) {
        iba_0 = 13;
        iba_1 = 7;
        iba_2 = 8;
        ibb_0 = 14;
        ibb_1 = 6;

        // incoming neutron
      } else if (ip1 == 14) {
        iba_0 = 14;
        iba_1 = 7;
        iba_2 = 8;
        ibb_0 = 13;
        ibb_1 = 6;
      }
      for (int j = 0; j < np; ++j) {
        if (LLIST[j] == iba_0) ima_0++;
        if (LLIST[j] == iba_1) ima_1++;
        if (LLIST[j] == iba_2) ima_2++;
        if (LLIST[j] == ibb_0) imb_0++;
        if (LLIST[j] == ibb_1) imb_1++;
      }

      // N gamma --> N pi +  pi- pi0
      if (ima_0 * ima_1 * ima_2 * imb_1 == 1) {
        double Elog = std::log(Ecm);
        double Elog_2 = std::log(E_ref_2);
        double Elog_1 = std::log(E_ref_1);
        double prob = 0.1 * 4. / std::pow(Elog_2 - Elog_1, 2) * (Elog - Elog_1) * (Elog_2 - Elog);
        if (RNDM() < prob) {
          if (ip1 == 13) {
            LL[0] = 14;
            LL[1] = 7;
            LL[2] = 7;
            LL[3] = 8;
          } else {
            LL[0] = 13;
            LL[1] = 7;
            LL[2] = 8;
            LL[3] = 8;
          }
          ND = 4;
        }
      }
    }

    if (ND > 0) {
      P_in[0] = 0.;
      P_in[1] = 0.;
      P_in[2] = 0.;
      P_in[3] = Ecm;
      P_in[4] = Ecm;
      DECPAR_zero_output dzo = DECPAR_zero(P_in, ND, LL);
      int Iflip = 0;
      for (int j = 0; j < ND; ++j) {
        LLIST[j] = LL[j];
        for (int k = 0; k < 5; ++k) {
          p[k][j] = dzo.P_out[k][j];
        }
        if ((LLIST[j] == 13 || LLIST[j] == 14) && p[2][j] < 0.) Iflip = 1;
      }
      if (Iflip != 0) {
        for (int j = 0; j < ND; ++j) {
          p[2][j] = -p[2][j];
        }
      }
      np = ND;
    }

    DECSIB();
  }

  int IQchr = ICHP[ip1 - 1] + ICHP[ip2 - 1];
  int IQbar = IBAR[ip1 - 1] + IBAR[ip2 - 1];
  check_event(-Ic, Ecm, 0., 0., 0., IQchr, IQbar);
  return;
}

void sophia_interface::DECSIB() {
  // ***********************************************************************
  // Decay all unstable particle in SIBYLL
  // decayed particle have the code increased by 10000
  // (taken from SIBYLL 1.7, R.E. 04/98)
  // ***********************************************************************
  int LLIST1[2000] = {0};
  double P0[5] = {0.};
  int NN = 1;
  while (NN <= np) {
    int L = LLIST[NN - 1];
    if (L == 0) throw std::runtime_error("DECSIB: L is never supposed to be zero.");
    if (IDB[std::abs(L) - 1] > 0) {
      for (int K = 0; K < 5; ++K) {
        P0[K] = p[K][NN - 1];
      }
      DECPAR_nonZero_output dno = DECPAR_nonZero(L, P0);
      int ND = dno.ND;
      int sgn = (LLIST[NN - 1] < 0) ? -1 : 1;
      LLIST[NN - 1] += sgn * 10000;
      for (int J = 0; J < ND; ++J) {
        for (int K = 0; K < 5; ++K) {
          p[K][np + J] = dno.P_out[K][J];
        }
        LLIST[np + J] = dno.LL[J];
        LLIST1[np + J] = NN;
      }
      np += ND;
    }
    NN++;
  }
  return;
}

DECPAR_zero_output sophia_interface::DECPAR_zero(double P0[5], int ND, int LL[10]) {
  // the routine generates a phase space decay of a particle
  // with 5-momentum P0(1:5) into ND particles of codes LL(1:nd)
  // (taken from SIBYLL 1.7, muon decay corrected, R.E. 04/98)
  double P_out[5][10] = {0.};
  double PV[5][10] = {0.};
  double RORD[10] = {0.};
  double UE[3] = {0.};
  double BE[3] = {0.};
  const double FACN[8] = {2.,    5.,     15.,    60., 250.,
                          1500., 12000., 120000.};  // was in FORTRAN counted from position 3 to 10.
  const double PI = 3.1415927;

  // c.m.s. Momentum in two particle decays
  std::function<double(double, double, double)> PAWT = [&](double A, double B, double C) {
    return std::sqrt((A * A - (B + C) * (B + C)) * (A * A - (B - C) * (B - C))) / (2. * A);
  };

  // Phase space decay into the particles in the list
  int MAT = 0;
  int MBST = 0;
  double PS = 0.;
  for (int J = 0; J < ND; ++J) {
    P_out[4][J] = AM[std::abs(LL[J]) - 1];
    PV[4][J] = AM[std::abs(LL[J]) - 1];
    PS += P_out[4][J];
  }
  for (int J = 0; J < 4; ++J) {
    PV[J][0] = P0[J];
  }
  PV[4][0] = P0[4];

  // 140
  double WWTMAX = 0.;
  bool goto280 = (ND == 2);
  if (goto280 == false) {
    if (ND == 1) {
      for (int J = 0; J < 4; ++J) {
        P_out[J][0] = P0[J];
      }
      DECPAR_zero_output dzo;
      for (int i = 0; i < ND; ++i) {
        for (int j = 0; j < 5; ++j) {
          dzo.P_out[j][i] = P_out[j][i];
        }
      }
      return dzo;
    }

    // Calculate maximum weight for ND-particle decay
    WWTMAX = 1. / FACN[ND - 3];
    double PMAX = PV[4][0] - PS + P_out[4][ND - 1];
    double PMIN = 0.;
    for (int IL = ND - 1; IL > 0; --IL) {
      PMAX += P_out[4][IL - 1];
      PMIN += P_out[4][IL];
      WWTMAX *= PAWT(PMAX, PMIN, P_out[4][IL - 1]);
    }
  }

  // generation of the masses, compute weight, if rejected try again
  double WT = 0.;
  bool repeat240 = false;
  do {  // 240
    repeat240 = false;

    if (goto280 == false) {
      RORD[0] = 1.;
      for (int IL1 = 2; IL1 < ND; ++IL1) {
        double RSAV = RNDM();
        int IL2tmp = 0;
        for (int IL2 = IL1 - 1; IL2 > 0; --IL2) {
          IL2tmp = IL2;
          if (RSAV <= RORD[IL2 - 1]) break;
          RORD[IL2] = RORD[IL2 - 1];
        }
        RORD[IL2tmp] = RSAV;
      }

      RORD[ND - 1] = 0.;
      WT = 1.;
      for (int IL = ND - 1; IL > 0; --IL) {
        PV[4][IL - 1] = PV[4][IL] + P_out[4][IL - 1] + (RORD[IL - 1] - RORD[IL]) * (PV[4][0] - PS);
        WT *= PAWT(PV[4][IL - 1], PV[4][IL], P_out[4][IL - 1]);
      }
      if (WT < RNDM() * WWTMAX) {
        repeat240 = true;
        continue;
      }
    }

    // Perform two particle decays in respective cm frame
    goto280 = false;  // 280
    for (int IL = 1; IL < ND; ++IL) {
      double PA = PAWT(PV[4][IL - 1], PV[4][IL], P_out[4][IL - 1]);
      UE[2] = 2. * RNDM() - 1.;
      double PHI = 2. * PI * RNDM();
      double UT = std::sqrt(1. - UE[2] * UE[2]);
      UE[0] = UT * std::cos(PHI);
      UE[1] = UT * std::sin(PHI);
      for (int J = 0; J < 3; ++J) {
        P_out[J][IL - 1] = PA * UE[J];
        PV[J][IL] = -PA * UE[J];
      }
      P_out[3][IL - 1] = std::sqrt(PA * PA + P_out[4][IL - 1] * P_out[4][IL - 1]);
      PV[3][IL] = std::sqrt(PA * PA + PV[4][IL] * PV[4][IL]);
    }

    // Lorentz transform decay products to lab frame
    for (int J = 0; J < 4; ++J) {
      P_out[J][ND - 1] = PV[J][ND - 1];
    }
    for (int IL = ND - 1; IL > 0; --IL) {  // 340 (first)
      for (int J = 0; J < 3; ++J) {
        BE[J] = PV[J][IL - 1] / PV[3][IL - 1];
      }
      double GA = PV[3][IL - 1] / PV[4][IL - 1];
      for (int I = IL; I < ND + 1; ++I) {  // 340 (second)
        double BEP = BE[0] * P_out[0][I - 1] + BE[1] * P_out[1][I - 1] + BE[2] * P_out[2][I - 1];
        for (int J = 0; J < 3; ++J) {
          P_out[J][I - 1] += GA * (GA * BEP / (1. + GA) + P_out[3][I - 1]) * BE[J];
        }
        P_out[3][I - 1] = GA * (P_out[3][I - 1] + BEP);
      }
    }

    // Weak decays
    if (MAT == 1) {
      double F1 = P_out[3][1] * P_out[3][2] - P_out[0][1] * P_out[0][2] -
                  P_out[1][1] * P_out[1][2] - P_out[2][1] * P_out[2][2];
      if (MBST == 1) WT = P0[4] * P_out[3][0] * F1;
      if (MBST == 0)
        WT = F1 * (P_out[3][0] * P0[3] - P_out[0][0] * P0[0] - P_out[1][0] * P0[1] -
                   P_out[2][0] * P0[2]);
      double WTMAX = std::pow(P0[4], 4) / 16.;
      if (WT < RNDM() * WTMAX) {
        repeat240 = true;
        continue;
      }
    }
  } while (repeat240);

  // Boost back for rapidly moving particle
  if (MBST == 1) {
    for (int J = 0; J < 3; ++J) {
      BE[J] = P0[J] / P0[3];
    }
    double GA = P0[3] / P0[4];
    for (int I = 0; I < ND; ++I) {
      double BEP = BE[0] * P_out[0][I] + BE[1] * P_out[1][I] + BE[2] * P_out[2][I];
      for (int J = 0; J < 3; ++J) {
        P_out[J][I] + GA*(GA * BEP / (1. + GA) + P_out[3][I]) * BE[J];
      }
      P_out[3][I] = GA * (P_out[3][I] + BEP);
    }
  }

  DECPAR_zero_output dzo;
  for (int i = 0; i < ND; ++i) {
    for (int j = 0; j < 5; ++j) {
      dzo.P_out[j][i] = P_out[j][i];
    }
  }
  return dzo;
}

DECPAR_nonZero_output sophia_interface::DECPAR_nonZero(int LA, double P0[5]) {
  // ***********************************************************************
  // This subroutine generates the decay of a particle
  // with ID = LA, and 5-momentum P0(1:5)
  // into ND particles of 5-momenta P(j,1:5) (j=1:ND)
  // (taken from SIBYLL 1.7, muon decay corrected, R.E. 04/98)
  // ***********************************************************************
  int LL[10] = {0};
  double P_out[5][10] = {0.};
  double PV[5][10] = {0.};
  double RORD[10] = {0.};
  double UE[3] = {0.};
  double BE[3] = {0.};
  const double FACN[8] = {2.,    5.,     15.,    60., 250.,
                          1500., 12000., 120000.};  // was in FORTRAN counted from position 3 to 10.
  const double PI = 3.1415927;

  // c.m.s. Momentum in two particle decays
  std::function<double(double, double, double)> PAWT = [&](double A, double B, double C) {
    return std::sqrt((A * A - (B + C) * (B + C)) * (A * A - (B - C) * (B - C))) / (2. * A);
  };

  // Choose decay channel
  int L = std::abs(LA);
  int ND = 0;
  int IDC = IDB[L - 1] - 1;

  if (IDC + 1 <= 0) {
    DECPAR_nonZero_output dno;
    dno.ND = ND;
    for (int i = 0; i < ND; ++i) {
      dno.LL[i] = LL[i];
      for (int j = 0; j < 5; ++j) {
        dno.P_out[j][i] = P_out[j][i];
      }
    }
    return dno;
  }

  double RBR = RNDM();
  do {
    IDC++;
  } while (RBR > CBR[IDC - 1]);

  int KD = 6 * (IDC - 1) + 1;
  ND = KDEC[KD - 1];
  double MAT = KDEC[KD];

  int MBST = (MAT > 0 && P0[3] > 20. * P0[4]) ? 1 : 0;
  double PS = 0.;
  for (int J = 0; J < ND; ++J) {
    LL[J] = KDEC[KD + 1 + J];
    P_out[4][J] = AM[LL[J] - 1];
    PV[4][J] = AM[LL[J] - 1];
    PS += P_out[4][J];
  }
  for (int J = 0; J < 4; ++J) {
    PV[J][0] = 0.;
    if (MBST == 0) PV[J][0] = P0[J];
  }
  if (MBST == 1) PV[3][0] = P0[4];
  PV[4][0] = P0[4];

  double WT = 0.;
  double WWTMAX = 0.;
  bool goto280 = (ND == 2);
  if (goto280 == false) {
    if (ND == 1) {
      // return with one particle
      for (int J = 0; J < 4; ++J) {
        P_out[J][0] = P0[J];
      }
      DECPAR_nonZero_output dno;
      dno.ND = ND;
      for (int i = 0; i < ND; ++i) {
        dno.LL[i] = LL[i];
        for (int j = 0; j < 5; ++j) {
          dno.P_out[j][i] = P_out[j][i];
        }
      }
      return dno;
    }

    // Calculate maximum weight for ND-particle decay
    WWTMAX = 1. / FACN[ND - 3];
    double PMAX = PV[4][0] - PS + P_out[4][ND - 1];
    double PMIN = 0.;
    for (int IL = ND - 1; IL > 0; --IL) {
      PMAX += P_out[4][IL - 1];
      PMIN += P_out[4][IL];
      WWTMAX *= PAWT(PMAX, PMIN, P_out[4][IL - 1]);
    }
  }

  // 280 continuing into loop
  // generation of the masses, compute weight, if rejected try again
  bool repeat240 = false;
  do {  // 240
    repeat240 = false;

    if (goto280 == false) {
      RORD[0] = 1.;
      for (int IL1 = 2; IL1 < ND; ++IL1) {
        double RSAV = RNDM();
        int IL2tmp = 0;
        for (int IL2 = IL1 - 1; IL2 > 0; --IL2) {
          IL2tmp = IL2;
          if (RSAV <= RORD[IL2 - 1]) break;
          RORD[IL2] = RORD[IL2 - 1];
        }
        RORD[IL2tmp] = RSAV;
      }

      RORD[ND - 1] = 0.;
      WT = 1.;
      for (int IL = ND - 1; IL > 0; --IL) {
        PV[4][IL - 1] = PV[4][IL] + P_out[4][IL - 1] + (RORD[IL - 1] - RORD[IL]) * (PV[4][0] - PS);
        WT *= PAWT(PV[4][IL - 1], PV[4][IL], P_out[4][IL - 1]);
      }
      if (WT < RNDM() * WWTMAX) {
        repeat240 = true;
        continue;
      }
    }
    goto280 = false;  // 280

    // Perform two particle decays in respective cm frame
    for (int IL = 1; IL < ND; ++IL) {
      double PA = PAWT(PV[4][IL - 1], PV[4][IL], P_out[4][IL - 1]);
      UE[2] = 2. * RNDM() - 1.;
      double PHI = 2. * PI * RNDM();
      double UT = std::sqrt(1. - UE[2] * UE[2]);
      UE[0] = UT * std::cos(PHI);
      UE[1] = UT * std::sin(PHI);
      for (int J = 0; J < 3; ++J) {
        P_out[J][IL - 1] = PA * UE[J];
        PV[J][IL] = -PA * UE[J];
      }
      P_out[3][IL - 1] = std::sqrt(PA * PA + P_out[4][IL - 1] * P_out[4][IL - 1]);
      PV[3][IL] = std::sqrt(PA * PA + PV[4][IL] * PV[4][IL]);
    }

    // Lorentz transform decay products to lab frame
    for (int J = 0; J < 4; ++J) {
      P_out[J][ND - 1] = PV[J][ND - 1];
    }

    for (int IL = ND - 1; IL > 0; --IL) {  // 340 (first)
      for (int J = 0; J < 3; ++J) {
        BE[J] = PV[J][IL - 1] / PV[3][IL - 1];
      }
      double GA = PV[3][IL - 1] / PV[4][IL - 1];
      for (int I = IL; I < ND + 1; ++I) {  // 340 (second)
        double BEP = BE[0] * P_out[0][I - 1] + BE[1] * P_out[1][I - 1] + BE[2] * P_out[2][I - 1];
        for (int J = 0; J < 3; ++J) {
          P_out[J][I - 1] += GA * (GA * BEP / (1. + GA) + P_out[3][I - 1]) * BE[J];
        }
        P_out[3][I - 1] = GA * (P_out[3][I - 1] + BEP);
      }
    }

    // Weak decays
    if (MAT == 1) {
      double F1 = P_out[3][1] * P_out[3][2] - P_out[0][1] * P_out[0][2] -
                  P_out[1][1] * P_out[1][2] - P_out[2][1] * P_out[2][2];
      if (MBST == 1) WT = P0[4] * P_out[3][0] * F1;
      if (MBST == 0)
        WT = F1 * (P_out[3][0] * P0[3] - P_out[0][0] * P0[0] - P_out[1][0] * P0[1] -
                   P_out[2][0] * P0[2]);
      double WTMAX = std::pow(P0[4], 4) / 16.;
      if (WT < RNDM() * WTMAX) {
        repeat240 = true;
        continue;
      }
    }
  } while (repeat240);

  // Boost back for rapidly moving particle
  if (MBST == 1) {
    for (int J = 0; J < 3; ++J) {
      BE[J] = P0[J] / P0[3];
    }
    double GA = P0[3] / P0[4];
    for (int I = 0; I < ND; ++I) {
      double BEP = BE[0] * P_out[0][I] + BE[1] * P_out[1][I] + BE[2] * P_out[2][I];
      for (int J = 0; J < 3; ++J) {
        P_out[J][I] += GA * (GA * BEP / (1. + GA) + P_out[3][I]) * BE[J];
      }
      P_out[3][I] = GA * (P_out[3][I] + BEP);
    }
  }

  // labels for antiparticle decay
  if (LA < 0 && L > 18) {
    for (int J = 0; J < ND; ++J) {
      LL[J] = LBARP[LL[J] - 1];
    }
  }

  DECPAR_nonZero_output dno;
  dno.ND = ND;
  for (int i = 0; i < ND; ++i) {
    dno.LL[i] = LL[i];
    for (int j = 0; j < 5; ++j) {
      dno.P_out[j][i] = P_out[j][i];
    }
  }
  return dno;
}

PO_MSHELL_output sophia_interface::PO_MSHELL(double PA1[4], double PA2[4], double XM1, double XM2) {
  // ********************************************************************
  // rescaling of momenta of two partons to put both on mass shell
  // input:       PA1,PA2   input momentum vectors
  //              XM1,2     desired masses of particles afterwards
  //              P1,P2     changed momentum vectors
  // (taken from PHOJET 1.12, R.E. 08/98)
  // ********************************************************************
  std::vector<double> P1;
  std::vector<double> P2;

  // Lorentz transformation into system CMS
  double PX = PA1[0] + PA2[0];
  double PY = PA1[1] + PA2[1];
  double PZ = PA1[2] + PA2[2];
  double EE = PA1[3] + PA2[3];
  double XMS = std::sqrt(EE * EE - PX * PX - PY * PY - PZ * PZ);
  double BGX = PX / XMS;
  double BGY = PY / XMS;
  double BGZ = PZ / XMS;
  double GAM = EE / XMS;

  PO_ALTRA_output pao = PO_ALTRA(GAM, -BGX, -BGY, -BGZ, PA1[0], PA1[1], PA1[2], PA1[3]);
  double PTOT1 = pao.P;
  P1.push_back(pao.PX);
  P1.push_back(pao.PY);
  P1.push_back(pao.PZ);
  P1.push_back(pao.E);

  // rotation angles
  const double DEPS = 1e-5;
  PTOT1 = std::max(DEPS, PTOT1);
  double COD = P1[2] / PTOT1;
  double SID = std::sqrt((1. - COD) * (1. + COD));
  double COF = 1.;
  double SIF = 0.;
  if (PTOT1 * SID > 1e-5) {
    COF = P1[0] / (SID * PTOT1);
    SIF = P1[1] / (SID * PTOT1);
    double ANORF = std::sqrt(COF * COF + SIF * SIF);
    COF /= ANORF;
    SIF /= ANORF;
  }

  // new CM momentum and energies (for masses XM1,XM2)
  double XM12 = XM1 * XM1;
  double XM22 = XM2 * XM2;
  double SS = XMS * XMS;
  double PCMP = PO_XLAM(SS, XM12, XM22) / (2. * XMS);
  double EE1 = std::sqrt(XM12 + PCMP * PCMP);
  double EE2 = XMS - EE1;

  // back rotation
  std::vector<double> PO_TRANS_output = PO_TRANS(0., 0., PCMP, COD, SID, COF, SIF);
  double XX = PO_TRANS_output[0];
  double YY = PO_TRANS_output[1];
  double ZZ = PO_TRANS_output[2];

  P1.clear();
  pao = PO_ALTRA(GAM, BGX, BGY, BGZ, XX, YY, ZZ, EE1);
  P1.push_back(pao.PX);
  P1.push_back(pao.PY);
  P1.push_back(pao.PZ);
  P1.push_back(pao.E);

  pao = PO_ALTRA(GAM, BGX, BGY, BGZ, -XX, -YY, -ZZ, EE2);
  P2.push_back(pao.PX);
  P2.push_back(pao.PY);
  P2.push_back(pao.PZ);
  P2.push_back(pao.E);

  PO_MSHELL_output pmo;
  pmo.P1 = P1;
  pmo.P2 = P2;
  return pmo;
}

std::vector<int> sophia_interface::valences(int ip) {
  // valence quark composition of various particles  (R.E. 03/98)
  // (with special treatment of photons)

  int ival1 = 0;
  int ival2 = 0;

  if (ip == 1) {
    if (RNDM() > 0.2) {
      ival1 = 1;
      ival2 = -1;
    } else {
      ival1 = 2;
      ival2 = -2;
    }
  } else if (ip == 6) {
    if (RNDM() > 0.5) {
      ival1 = 1;
      ival2 = -1;
    } else {
      ival1 = 2;
      ival2 = -2;
    }
  } else if (ip == 7) {
    ival1 = 1;
    ival2 = -2;
  } else if (ip == 8) {
    ival1 = 2;
    ival2 = -1;
  } else if (ip == 13) {
    double Xi = RNDM();
    if (Xi < 0.3333) {
      ival1 = 12;
      ival2 = 1;
    } else if (Xi < 0.6666) {
      ival1 = 21;
      ival2 = 1;
    } else {
      ival1 = 11;
      ival2 = 2;
    }
  } else if (ip == 14) {
    double Xi = RNDM();
    if (Xi < 0.3333) {
      ival1 = 12;
      ival2 = 2;
    } else if (Xi < 0.6666) {
      ival1 = 21;
      ival2 = 2;
    } else {
      ival1 = 22;
      ival2 = 1;
    }
  }
  if (ip < 13 && RNDM() < 0.5) {
    int k = ival1;
    ival1 = ival2;
    ival2 = k;
  }

  std::vector<int> output;
  output.push_back(ival1);
  output.push_back(ival2);
  return output;
}

void sophia_interface::check_event(int Ic, double Esum, double PXsum, double PYsum, double PZsum,
                                   int IQchr, int IQbar) {
  // check energy-momentum and quantum number conservation (R.E. 08/98)
  double px = 0.;
  double py = 0.;
  double pz = 0.;
  double ee = 0.;
  int ichar = 0;
  int ibary = 0;
  bool hasFailed = false;

  double PLscale = Esum;
  double PTscale = 1.;

  for (int j = 0; j < np; ++j) {
    int l1 = std::abs(LLIST[j]);
    int l = LLIST[j] % 10000;
    if (l1 < 10000) {
      px += p[0][j];
      py += p[1][j];
      pz += p[2][j];
      ee += p[3][j];
      int sgn = (l < 0) ? -1 : 1;
      ichar += sgn * ICHP[std::abs(l) - 1];
      ibary += sgn * IBAR[std::abs(l) - 1];
    }
  }

  if (ichar != IQchr) {
    std::cout << " charge conservation violated: Ic = " << Ic << std::endl;
    hasFailed = true;
  }
  if (ibary != IQbar) {
    std::cout << " baryon number conservation violated: Ic = " << Ic << std::endl;
    hasFailed = true;
  }
  if (std::abs((px - PXsum) / std::max(PXsum, PTscale)) > 1.e-3) {
    std::cout << " x momentum conservation violated: Ic = " << Ic << std::endl;
    hasFailed = true;
  }
  if (std::abs((py - PYsum) / std::max(PYsum, PTscale)) > 1.e-3) {
    std::cout << " y momentum conservation violated: Ic = " << Ic << std::endl;
    hasFailed = true;
  }
  if (std::abs((pz - PZsum) / std::max(std::abs(PZsum), PLscale)) > 1.e-3) {
    std::cout << " z momentum conservation violated: Ic = " << Ic << std::endl;
    hasFailed = true;
  }
  if (std::abs((ee - Esum) / std::max(Esum, 1.)) > 1.e-3) {
    std::cout << " energy conservation violated: Ic = " << Ic << std::endl;
    hasFailed = true;
  }
  // debugNonLUND("check_event", false);
  if (hasFailed) throw std::runtime_error("check_event has failed critically");

  return;
}

int sophia_interface::dec_res2(double eps_prime, int IRESMAX, int L0) {
  // *****************************************************************************
  // decides which resonance with ID=IRES in list takes place at eps_prime
  // ** Date: 20/01/98   **
  // ** author: A.Muecke **
  // **********************
  double prob_sum[9] = {0.};

  // sum of all resonances:
  double sumres = 0.;
  for (int j = 1; j < IRESMAX + 1; ++j) {
    int j10 = j + 10;
    sumres += crossection(eps_prime, j10, L0);
    prob_sum[j - 1] = sumres;
  }

  int IRES = 0;
  double r = RNDM();
  int i = 0;
  double prob = 0.;
  do {
    i++;
    double probold = prob;
    prob = prob_sum[i - 1] / sumres;
    if (r >= probold && r < prob) {
      IRES = i;
      return IRES;
    }
  } while (i < IRESMAX);
  if (r == 1.) {
    IRES = i;
    return IRES;
  }
  if (IRES == 0) {
    throw std::runtime_error("dec_res2: no resonance possible");
  }
  return IRES;
}

void sophia_interface::RES_DECAY3(int IRES, int IPROC, int IRANGE, double s, int L0) {
  // ********************************************************
  // RESONANCE AMD with code number IRES  INTO  M1 + M2
  // PROTON ENERGY E0 [in GeV] IN DMM [in GeV]
  // E1,E2 [in GeV] are energies of decay products
  // LA,LB are code numbers of decay products
  // P(1,1:5),P(2,1:5) are 5-momenta of particles LA,LB;
  // resulting momenta are calculated in CM frame;
  // cosAngle is cos of scattering angle in CM frame
  // ********************************************************
  // ** Date: 20/01/98    **
  // ** correct.:28/04/98 **
  // ** author: A.Muecke  **
  // **********************
  // determine decay products LA, LB:
  np = 2;

  int LA = 0;
  int LB = 0;
  if (L0 == 13) {
    // proton is incident nucleon:
    if (IRANGE == 1) {
      LA = KDECRES1p[5 * (IPROC - 1) + 2];
      LB = KDECRES1p[5 * (IPROC - 1) + 3];
    } else if (IRANGE == 2) {
      LA = KDECRES2p[5 * (IPROC - 1) + 2];
      LB = KDECRES2p[5 * (IPROC - 1) + 3];
    } else if (IRANGE == 3) {
      LA = KDECRES3p[5 * (IPROC - 1) + 2];
      LB = KDECRES3p[5 * (IPROC - 1) + 3];
    } else {
      throw std::runtime_error("error in RES_DECAY3");
    }
  } else if (L0 == 14) {
    // neutron is incident nucleon:
    if (IRANGE == 1) {
      LA = KDECRES1n[5 * (IPROC - 1) + 2];
      LB = KDECRES1n[5 * (IPROC - 1) + 3];
    } else if (IRANGE == 2) {
      LA = KDECRES2n[5 * (IPROC - 1) + 2];
      LB = KDECRES2n[5 * (IPROC - 1) + 3];
    } else if (IRANGE == 3) {
      LA = KDECRES3n[5 * (IPROC - 1) + 2];
      LB = KDECRES3n[5 * (IPROC - 1) + 3];
    } else {
      throw std::runtime_error("error in RES_DECAY3");
    }

  } else {
    throw std::runtime_error("no valid L0 in RES_DECAY3");
  }

  LLIST[0] = LA;
  LLIST[1] = LB;

  // sample scattering angle:
  double cosAngle = scatterangle(IRES, L0);

  // 2-particle decay:
  PROC_TWOPART(LA, LB, std::sqrt(s), cosAngle);

  return;
}

void sophia_interface::PROC_TWOPART(int LA, int LB, double AMD, double cosAngle) {
  // ***********************************************************
  // 2-particle decay of CMF mass AMD INTO  M1 + M2
  // nucleon energy E0 [in GeV];
  // E1,E2 [in GeV] are energies of decay products
  // LA,LB are code numbers of decay products
  // P1(1:5),P2(1:5) are 5-momenta of particles LA,LB;
  // resulting momenta are calculated in CM frame;
  // costheta is cos of scattering angle in CM frame
  // this program also checks if the resulting particles are
  // resonances; if yes, it is also allowed to decay a
  // mass AMD < M1 + M2 by using the width of the resonance(s)
  // ***********************************************************
  // ** Date: 20/01/98    **
  // ** correct.:19/02/98 **
  // ** author: A.Muecke  **
  // **********************
  int nbad = 0;
  double SM1 = AM[LA - 1];
  double SM2 = (LB == 0) ? 2. * AM[6] : AM[LB - 1];

  double E1 = (AMD * AMD + SM1 * SM1 - SM2 * SM2) / AMD / 2.;
  double E2 = (AMD * AMD + SM2 * SM2 - SM1 * SM1) / AMD / 2.;

  // check if SM1+SM2 < AMD:
  if (SM1 + SM2 > AMD) {
    // if one of the decay products is a resonance, this 'problem' can
    // be solved by using a reduced mass for the resonance and assume that
    // this resonance is produced at its threshold;
    if (FRES[LA - 1] == 1.) {
      // particle LA is a resonance:
      SM1 = AMD - SM2;
      E1 = SM1;
      E2 = AMD - E1;
      if (E1 < XLIMRES[LA - 1] || E2 < XLIMRES[LB - 1]) nbad = 1;
    }
    if (FRES[LB - 1] == 1.) {
      // particle LB is a resonance:
      SM2 = AMD - SM1;
      E2 = SM2;
      E1 = AMD - E2;
      if (E1 < XLIMRES[LA - 1] || E2 < XLIMRES[LB - 1]) nbad = 1;
    }
    // both particles are NOT resonances: -> error
    if (FRES[LA - 1] == 0. && FRES[LB - 1] == 0.) {
      throw std::runtime_error("error in function PROC_TWOPART");
    }
  }

  if (nbad == 0) {
    double PC = std::sqrt(E1 * E1 - SM1 * SM1);
    p[3][0] = E1;
    p[3][1] = E2;
    p[4][0] = SM1;
    p[4][1] = SM2;

    // theta is scattering angle in CM frame:
    double r = RNDM();
    double P1Z = PC * cosAngle;
    double P2Z = -PC * cosAngle;
    double P1X = std::sqrt(r * (PC * PC - P1Z * P1Z));
    double P2X = std::sqrt(r * (PC * PC - P2Z * P2Z));
    double P1Y = std::sqrt((1. - r) * (PC * PC - P1Z * P1Z));
    double P2Y = std::sqrt((1. - r) * (PC * PC - P2Z * P2Z));
    if (RNDM() < 0.5) {
      P1X = -P1X;
    } else {
      P2X = -P2X;
    }
    if (RNDM() < 0.5) {
      P1Y = -P1Y;
    } else {
      P2Y = -P2Y;
    }

    p[0][0] = P1X;
    p[1][0] = P1Y;
    p[2][0] = P1Z;
    p[0][1] = P2X;
    p[1][1] = P2Y;
    p[2][1] = P2Z;
    LLIST[0] = LA;
    LLIST[1] = LB;
  } else {
    throw std::runtime_error("error in function PROC_TWOPART: nbad != 0");
  }

  return;
}

int sophia_interface::dec_inter3(double eps_prime, int L0) {
  // *** decides which process takes place at eps_prime *********
  // (0) multipion production (fragmentation)                 ***
  // (1) diffractive scattering: N\gamma --> N \rho           ***
  // (2) direct pion production: N\gamma --> N \pi            ***
  // (3) direct pion production: N\gamma --> \Delta \pi       ***
  // (4) diffractive scattering: N\gamma --> N \omega         ***
  // (5) fragmentation in resonance region                    ***
  // (6) excitation/decay of resonance                        ***
  // ************************************************************
  // ** Date: 15/04/98   **
  // ** author: A.Muecke **
  // **********************
  int Imode = -1;

  double tot = crossection(eps_prime, 3, L0);
  if (tot == 0.) tot = 1.;
  const double prob1 = crossection(eps_prime, 1, L0) / tot;
  const double prob2 = crossection(eps_prime, 7, L0) / tot;
  const double prob3 = crossection(eps_prime, 2, L0) / tot;
  const double prob4 = crossection(eps_prime, 8, L0) / tot;
  const double prob5 = crossection(eps_prime, 9, L0) / tot;
  const double prob6 = crossection(eps_prime, 0, L0) / tot;
  const double rn = RNDM();
  if (rn < prob1) {
    Imode = 6;  // --> resonance decay
  } else if (prob1 <= rn && rn < prob2) {
    Imode = 2;  // --> direct channel: N\gamma --> N\pi
  } else if (prob2 <= rn && rn < prob3) {
    Imode = 3;  // --> direct channel: N\gamma --> \Delta \pi
  } else if (prob3 <= rn && rn < prob4) {
    Imode = 1;  // --> diffractive scattering: N\gamma --> N \rho
  } else if (prob4 <= rn && rn < prob5) {
    Imode = 4;  // --> diffractive scattering: N\gamma --> N \omega
  } else if (prob5 <= rn && rn < prob6) {
    Imode = 5;  // --> fragmentation (2) in resonance region
  } else if (prob6 <= rn && rn < 1.) {
    Imode = 0;  // --> fragmentation mode/multipion production
  } else if (rn == 1.) {
    Imode = 0;
  } else {
    throw std::runtime_error("error in dec_inter3");
  }
  return Imode;
}

double sophia_interface::sample_s(double eps, int L0, double Ein) {
  // ***********************************************************************
  //  samples distribution of s: p(s) = (s-mp^2)sigma_Ngamma
  //  rejection for s=[sth,s0], analyt.inversion for s=[s0,smax]
  // ***********************************************************************
  // ** Date: 20/01/98   **
  // ** author: A.Muecke **
  // **********************
  double s = 0.;
  double xmp = AM[L0 - 1];
  double Pp = std::sqrt(Ein * Ein - xmp * xmp);
  double smin = 1.1646;
  double smax = std::max(smin, xmp * xmp + 2. * eps * (Ein + Pp));
  if (smax - smin <= 1e-8) {
    s = smin + RNDM() * 1e-6;
    return s;
  }

  // determine which method applies: rejection or analyt. inversion:
  double s0 = 10.;
  double sintegr1 = gaussInt([this, L0](double s) { return this->functs(s, L0); }, smin, s0);
  double sintegr2 = gaussInt([this, L0](double s) { return this->functs(s, L0); }, s0, smax);

  int nmethod = 0;
  if (smax <= s0) {
    // rejection method:
    nmethod = 1;
  } else {
    double quo = sintegr1 / (sintegr1 + sintegr2);
    nmethod = (RNDM() <= quo) ? 1 : 2;  // 1: rejection method, 2: analyt. inversion
  }

  // rejection method: ************************
  if (nmethod == 1) {
    int i_rept = 0;
    double ps = 0.;
    // pmax is roughly p(s) at s=s0
    double pmax = 1300. / sintegr1;
    do {
      // sample s random between smin ... s0 **
      if (i_rept >= 100000) return s;
      s = smin + RNDM() * (smax - smin);
      // calculate p(s) = pes **********************
      ps = functs(s, L0);
      // rejection method to sample s *********************
      i_rept++;
    } while (RNDM() * pmax > ps / sintegr1);
    return s;
  }

  // analyt. inversion method: *******************
  if (nmethod == 2) {
    double r4 = RNDM();
    double beta = 2.04;
    double betai = 1. / beta;
    double term1 = r4 * std::pow(smax, beta);
    double term2 = (r4 - 1.) * std::pow(s0, beta);
    s = std::pow(term1 - term2, betai);
    return s;
  }

  throw std::runtime_error("error in function sample_s");
}

double sophia_interface::functs(double s, int L0) {
  // Returns (s-pm^2)*sigma_Nucleon/gamma
  const double pm = 0.93827;  // Gev/c^2
  double factor = s - pm * pm;
  double epsprime = factor / 2. / pm;
  double sigma_pg = crossection(epsprime, 3, L0);
  return factor * sigma_pg;
}

double sophia_interface::crossection(double x, int NDIR, int NL0) {
  // calculates crossection of Nucleon-gamma-interaction
  // (see thesis of J.Rachen, p.45ff and corrections
  // report from 27/04/98, 5/05/98, 22/05/98 of J.Rachen)
  // ** Date: 20/01/98   **
  // ** correct.:27/04/98**
  // ** update: 23/05/98 **
  // ** author: A.Muecke **

  if (NL0 != 13 && NL0 != 14)
    throw std::runtime_error("crossection: particle ID incorrectly specified.");
  const double AM2 =
      (NL0 == 13)
          ? 0.880351
          : 0.882792;  // used to be array AM2[49]. Only these two values of it are ever used.
  double SIG0[9] = {0.};
  double AMRES[9] = {0.};
  double WIDTH[9] = {0.};

  if (NL0 == 13) {
    for (int i = 0; i < 9; ++i) {
      SIG0[i] = 4.893089117 / AM2 * RATIOJp[i] * BGAMMAp[i];
      AMRES[i] = AMRESp[i];
      WIDTH[i] = WIDTHp[i];
    }
  }

  if (NL0 == 14) {
    for (int i = 0; i < 9; ++i) {
      SIG0[i] = 4.893089117 / AM2 * RATIOJn[i] * BGAMMAn[i];
      AMRES[i] = AMRESn[i];
      WIDTH[i] = WIDTHn[i];
    }
  }

  double sig_res[9] = {0.};

  const double sth = 1.1646;
  const double pm = AM[NL0 - 1];
  const double s = pm * pm + 2. * pm * x;
  if (s < sth) {
    return 0.;
  }
  double cross_res = 0.;
  double cross_dir = 0.;
  double cross_dir1 = 0.;
  double cross_dir2 = 0.;
  if (x <= 10.) {
    // RESONANCES:
    cross_res = breitwigner(SIG0[0], WIDTH[0], AMRES[0], x) * Ef(x, 0.152, 0.17);
    sig_res[0] = cross_res;

    for (int Ni = 1; Ni < 9; ++Ni) {
      sig_res[Ni] = breitwigner(SIG0[Ni], WIDTH[Ni], AMRES[Ni], x) * Ef(x, 0.15, 0.38);
      cross_res += sig_res[Ni];
    }
    // DIRECT CHANNEL:
    if (x > 0.1 && x < 0.6) {
      cross_dir1 = singleback(x) + 40. * std::exp(-(x - 0.29) * (x - 0.29) / 0.002) -
                   15. * std::exp(-(x - 0.37) * (x - 0.37) / 0.002);
    } else {
      cross_dir1 = singleback(x);
    }
    cross_dir2 = twoback(x);
    cross_dir = cross_dir1 + cross_dir2;
  }

  // FRAGMENTATION 2:
  double cross_frag2 = 0.;
  if (NL0 == 13) {
    cross_frag2 = 80.3 * Ef(x, 0.5, 0.1) * std::pow(s, -0.34);
  } else if (NL0 == 14) {
    cross_frag2 = 60.2 * Ef(x, 0.5, 0.1) * std::pow(s, -0.34);
  }

  // MULTIPION PRODUCTION/FRAGMENTATION 1 CROSS SECTION
  double cross_diffr = 0.;
  double cross_diffr1 = 0.;
  double cross_diffr2 = 0.;
  double cs_multidiff = 0.;
  double cs_multi = 0.;
  if (x > 0.85) {
    double ss1 = (x - .85) / .69;
    double ss2 = 0.;
    if (NL0 == 13) {
      ss2 = 29.3 * std::pow(s, -.34) + 59.3 * std::pow(s, .095);
    } else if (NL0 == 14) {
      ss2 = 26.4 * std::pow(s, -.34) + 59.3 * std::pow(s, .095);
    }
    cs_multidiff = (1. - std::exp(-ss1)) * ss2;
    cs_multi = 0.89 * cs_multidiff;

    // DIFFRACTIVE SCATTERING:
    cross_diffr1 = .099 * cs_multidiff;
    cross_diffr2 = .011 * cs_multidiff;
    cross_diffr = 0.11 * cs_multidiff;

    ss1 = std::pow((x - .85), .75) / .64;
    ss2 = 74.1 * std::pow(x, -.44) + 62. * std::pow(s, .08);
    double cs_tmp = 0.96 * (1. - std::exp(-ss1)) * ss2;
    cross_diffr1 = 0.14 * cs_tmp;
    cross_diffr2 = 0.013 * cs_tmp;
    double cs_delta = cross_frag2 - (cross_diffr1 + cross_diffr2 - cross_diffr);
    if (cs_delta < 0.) {
      cross_frag2 = 0.;
      cs_multi += cs_delta;
    } else {
      cross_frag2 = cs_delta;
    }
    cross_diffr = cross_diffr1 + cross_diffr2;
    cs_multidiff = cs_multi + cross_diffr;
  }

  double xSection = 0.;
  switch (NDIR) {
    case 0:
      xSection = cross_res + cross_dir + cross_diffr + cross_frag2;
      break;
    case 1:
      xSection = cross_res;
      break;
    case 2:
      xSection = cross_res + cross_dir;
      break;
    case 3:
      xSection = cross_res + cross_dir + cs_multidiff + cross_frag2;
      break;
    case 4:
      xSection = cross_dir;
      break;
    case 5:
      xSection = cs_multi;
      break;
    case 6:
      xSection = cross_res + cross_dir2;
      break;
    case 7:
      xSection = cross_res + cross_dir1;
      break;
    case 8:
      xSection = cross_res + cross_dir + cross_diffr1;
      break;
    case 9:
      xSection = cross_res + cross_dir + cross_diffr;
      break;
    case 10:
      xSection = cross_diffr;
      break;
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
      xSection = sig_res[NDIR - 11];
      break;
    default:
      throw std::runtime_error("wrong input NDIR in crossection.f !");
  }
  return xSection;
}

dec_proc2_output sophia_interface::dec_proc2(double x, int IRES, int L0) {
  // decide which decay with ID=IPROC of resonance IRES takes place ***
  // Date: 20/01/98   **
  // correct.: 27/04/98*
  // author: A.Muecke **
  int IPROC = 0;
  int IRANGE = 0;
  double prob_sum[10] = {0.};

  // choose arrays /S_RESp/ for charged resonances,
  //        arrays /S_RESn/ for neutral resonances
  if (L0 == 13) {
    // charged resonances:
    double r = RNDM();
    // determine the energy range of the resonance:
    int nlim = ELIMITSp[IRES - 1];
    int istart = (IRES - 1) * 4 + 1;
    if (nlim > 0) {
      for (int ie = istart; ie < nlim - 2 + istart + 1; ++ie) {
        double reslimp1 = RESLIMp[ie - 1];
        double reslimp2 = RESLIMp[ie];
        if (x <= reslimp2 && x > reslimp1) {
          IRANGE = ie + 1 - istart;
        }
      }
    } else {
      IRANGE = 1;
    }

    IPROC = -1;
    int i = 0;
    int j = 0;
    prob_sum[0] = 0.;
    if (IRANGE == 1) {
      j = IDBRES1p[IRES - 1] - 1;
      if (j == -1) {
        std::runtime_error("invalid resonance in energy range 1");
      }
      do {
        j++;
        i++;
        prob_sum[i] = CBRRES1p[j - 1];
        if (r >= prob_sum[i - 1] && r < prob_sum[i]) IPROC = j;
      } while (prob_sum[i] < 1.);
      if (r == 1.) IPROC = j;
      if (IPROC == -1) {
        std::runtime_error("no resonance decay possible !");
      }
    } else if (IRANGE == 2) {
      j = IDBRES2p[IRES - 1] - 1;
      if (j == -1) {
        std::runtime_error("invalid resonance in energy range 2");
      }
      do {
        j++;
        i++;
        prob_sum[i] = CBRRES2p[j - 1];
        if (r >= prob_sum[i - 1] && r < prob_sum[i]) IPROC = j;
      } while (prob_sum[i] < 1.);
      if (r == 1.) IPROC = j;
      if (IPROC == -1) {
        std::runtime_error("no resonance decay possible !");
      }
    } else if (IRANGE == 3) {
      j = IDBRES3p[IRES - 1] - 1;
      if (j == -1) {
        std::runtime_error("invalid resonance in energy range 3");
      }
      do {
        j++;
        i++;
        prob_sum[i] = CBRRES3p[j - 1];
        if (r >= prob_sum[i - 1] && r < prob_sum[i]) IPROC = j;
      } while (prob_sum[i] < 1.);
      if (r == 1.) IPROC = j;
      if (IPROC == -1) {
        std::runtime_error("no resonance decay possible !");
      }
    } else {
      std::runtime_error("invalid IRANGE in DEC_PROC2");
    }

    // output
    dec_proc2_output dpo;
    dpo.IPROC = IPROC;
    dpo.IRANGE = IRANGE;
    return dpo;
  } else if (L0 == 14) {
    // neutral resonances:
    double r = RNDM();
    // determine the energy range of the resonance:
    int nlim = ELIMITSn[IRES - 1];
    int istart = (IRES - 1) * 4 + 1;
    if (nlim > 0) {
      for (int ie = istart; ie < nlim - 2 + istart + 1; ++ie) {
        if (x <= RESLIMn[ie] && x > RESLIMn[ie - 1]) {
          IRANGE = ie + 1 - istart;
        }
      }
    } else {
      IRANGE = 1;
    }
    IPROC = -1;
    int i = 0;
    int j = 0;
    prob_sum[0] = 0.;
    if (IRANGE == 1) {
      j = IDBRES1n[IRES - 1] - 1;
      if (j == -1) {
        std::runtime_error("invalid resonance in this energy range");
      }
      do {
        j++;
        i++;
        prob_sum[i] = CBRRES1n[j - 1];
        if (r >= prob_sum[i - 1] && r < prob_sum[i]) IPROC = j;
      } while (prob_sum[i] < 1.);
      if (r == 1.) IPROC = j;
      if (IPROC == -1) {
        std::runtime_error("no resonance decay possible !");
      }
    } else if (IRANGE == 2) {
      j = IDBRES2n[IRES - 1] - 1;
      if (j == -1) {
        std::runtime_error("invalid resonance in this energy range");
      }
      do {
        j++;
        i++;
        prob_sum[i] = CBRRES2n[j - 1];
        if (r >= prob_sum[i - 1] && r < prob_sum[i]) IPROC = j;
      } while (prob_sum[i] < 1.);
      if (r == 1.) IPROC = j;
      if (IPROC == -1) {
        std::runtime_error("no resonance decay possible !");
      }
    } else if (IRANGE == 3) {
      j = IDBRES3n[IRES - 1] - 1;
      if (j == -1) {
        std::runtime_error("invalid resonance in this energy range");
      }
      do {
        j++;
        i++;
        prob_sum[i] = CBRRES3n[j - 1];
        if (r >= prob_sum[i - 1] && r < prob_sum[i]) IPROC = j;
      } while (prob_sum[i] < 1.);
      if (r == 1.) IPROC = j;
      if (IPROC == -1) {
        std::runtime_error("no resonance decay possible !");
      }
    } else {
      std::runtime_error("invalid IRANGE in DEC_PROC2");
    }
    // output
    dec_proc2_output dpo;
    dpo.IPROC = IPROC;
    dpo.IRANGE = IRANGE;
    return dpo;
  } else {
    std::runtime_error("no valid L0 in DEC_PROC !");
  }
}

double sophia_interface::singleback(double x) {
  // SINGLE PION CHANNEL
  return 92.7 * Pl(x, 0.152, 0.25, 2.);
}

double sophia_interface::twoback(double x) {
  // TWO PION PRODUCTION
  return 37.7 * Pl(x, 0.4, 0.6, 2.);
}

double sophia_interface::scatterangle(int IRES, int L0) {
  // *******************************************************************
  // This routine samples the cos of the scattering angle for a given **
  // resonance IRES and incident nucleon L0; it is exact for          **
  // one-pion decay channel and if there is no                        **
  // other contribution to the cross section from another resonance   **
  // and an approximation for an overlay of resonances;               **
  // for decay channels other than the one-pion decay a isotropic     **
  // distribution is used                                             **
  // *******************************************************************
  // ** Date: 16/02/98   **
  // ** author: A.Muecke **
  // **********************
  // use rejection method for sampling:
  int LA = LLIST[0];
  int LB = LLIST[1];

  double cosAngle = 0.;
  double prob = 0.;
  double r = 0.;
  do {
    double r = RNDM();
    // sample cosAngle randomly between -1 ... 1
    cosAngle = 2. * (r - 0.5);
    // distribution is isotropic for other than one-pion decays:
    if ((LA == 13 || LA == 14) && LB >= 6 && LB <= 8) {
      prob = probangle(IRES, L0, cosAngle);
    } else {
      prob = 0.5;
    }
    r = RNDM();
    if (r <= prob) return cosAngle;
  } while (true);
}

double sophia_interface::probangle(int IRES, int L0, double z) {
  // *******************************************************************
  // probability distribution for scattering angle of given resonance **
  // IRES and incident nucleon L0 ;                                   **
  // z is cosine of scattering angle in CMF frame                     **
  // *******************************************************************
  if (IRES == 4 || IRES == 5 || IRES == 2) {
    // N1535 andf N1650 decay isotropically.
    return 0.5;
  } else if (IRES == 1) {
    // for D1232:
    return 0.636263 - 0.408790 * z * z;
  } else if (IRES == 3 && L0 == 14) {
    // for N1520 and incident n:
    return 0.673669 - 0.521007 * z * z;
  } else if (IRES == 3 && L0 == 13) {
    // for N1520 and incident p:
    return 0.739763 - 0.719288 * z * z;
  } else if (IRES == 6 && L0 == 14) {
    // for N1680 (more precisely: N1675) and incident n:
    double q = z * z;
    return 0.254005 + 1.427918 * q - 1.149888 * q * q;
  } else if (IRES == 6 && L0 == 13) {
    // for N1680 and incident p:
    double q = z * z;
    return 0.189855 + 2.582610 * q - 2.753625 * q * q;
  } else if (IRES == 7) {
    // for D1700:
    return 0.450238 + 0.149285 * z * z;
  } else if (IRES == 8) {
    // for D1905:
    double q = z * z;
    return 0.230034 + 1.859396 * q - 1.749161 * q * q;
  } else if (IRES == 9) {
    // for D1950:
    double q = z * z;
    return 0.397430 - 1.498240 * q + 5.880814 * q * q - 4.019252 * q * q * q;
  }

  std::runtime_error("error in function probangle!");
}

PO_ALTRA_output sophia_interface::PO_ALTRA(double GA, double BGX, double BGY, double BGZ,
                                           double PCX, double PCY, double PCZ, double EC) {
  // *********************************************************************
  // arbitrary Lorentz transformation
  // (taken from PHOJET v1.12, R.E. 08/98)
  // *********************************************************************
  double EP = PCX * BGX + PCY * BGY + PCZ * BGZ;
  double PE = EP / (GA + 1.) + EC;
  double PX = PCX + BGX * PE;
  double PY = PCY + BGY * PE;
  double PZ = PCZ + BGZ * PE;
  double P = std::sqrt(PX * PX + PY * PY + PZ * PZ);
  double E = GA * EC + EP;

  PO_ALTRA_output pao;
  pao.PX = PX;
  pao.PY = PY;
  pao.PZ = PZ;
  pao.P = P;
  pao.E = E;
  return pao;
}

std::vector<double> sophia_interface::PO_TRANS(double XO, double YO, double ZO, double CDE,
                                               double SDE, double CFE, double SFE) {
  // *********************************************************************
  // rotation of coordinate frame (1) de rotation around y axis
  //                              (2) fe rotation around z axis
  // (taken from PHOJET v1.12, R.E. 08/98)
  // *********************************************************************
  double X = CDE * CFE * XO - SFE * YO + SDE * CFE * ZO;
  double Y = CDE * SFE * XO + CFE * YO + SDE * SFE * ZO;
  double Z = -SDE * XO + CDE * ZO;

  std::vector<double> output;
  output.push_back(X);
  output.push_back(Y);
  output.push_back(Z);
  return output;
}

PO_SELSX2_output sophia_interface::PO_SELSX2(double XMIN[2], double XMAX[2], double AS1,
                                             double AS2) {
  // *********************************************************************
  // select x values of soft string ends using PO_RNDBET
  // (taken from PHOJET v1.12, R.E. 08/98)
  // *********************************************************************
  PO_SELSX2_output pso;

  bool isRejected = true;

  double X1 = 0.;
  double X2 = 0.;
  double X3 = 0.;
  double X4 = 0.;

  for (int I = 0; I < 100; ++I) {
    int ITRY1 = 0;
    do {
      X1 = PO_RNDBET(2.5, 0.5);
      ITRY1++;
      if (ITRY1 >= 50) {
        pso.isRejected = true;
        return pso;
      }
    } while (X1 <= XMIN[0] || X1 >= XMAX[0]);

    int ITRY2 = 0;
    do {
      X2 = PO_RNDBET(0.5, 0.5);
      ITRY2++;
      if (ITRY2 >= 50) {
        pso.isRejected = true;
        return pso;
      }
    } while (X2 <= XMIN[1] || X2 >= XMAX[1]);

    X3 = 1. - X1;
    X4 = 1. - X2;
    if (X1 * X2 > AS1 && X3 * X4 > AS2) {
      isRejected = false;
      break;
    }
  }

  if (isRejected) {
    pso.isRejected = true;
    return pso;
  } else {
    pso.XS1[0] = X1;
    pso.XS1[1] = X3;
    pso.XS2[0] = X2;
    pso.XS2[1] = X4;
    pso.isRejected = false;
    return pso;
  }
}

double sophia_interface::PO_RNDBET(double GAM, double ETA) {
  // *********************************************************************
  // random number generation from beta
  // distribution in region  0 < X < 1.
  // F(X) = X**(GAM-1.)*(1.-X)**(ETA-1)*GAMM(ETA+GAM) / (GAMM(GAM*GAMM(ETA))
  // (taken from PHOJET v1.12, R.E. 08/98)
  // *********************************************************************
  double Y = PO_RNDGAM(GAM);
  double Z = PO_RNDGAM(ETA);
  return Y / (Y + Z);
}

double sophia_interface::PO_RNDGAM(double ETA) {
  // *********************************************************************
  // random number selection from gamma distribution
  // F(X) = ALAM**ETA*X**(ETA-1)*EXP(-ALAM*X) / GAM(ETA)
  // (taken from PHOJET v1.12, R.E. 08/98)

  // Note1: if you compare this function with its FORTRAN original version,
  // have a look at how GOTOs work there and how doubles are being cast into
  // integers and vice versa. The redundant code solves the GOTO pointers.
  // this was definetely one of the most exciting functions to translate!
  // Note2 ALAM would have been an argument to this function but is always called with ALAM=1.
  // *********************************************************************
  double Y = 0.;
  int n = static_cast<int>(ETA);
  double F = ETA - static_cast<double>(n);
  if (F != 0.) {
    int NCOU = 0;
    bool goto20 = false;
    bool repeat10 = false;
    do {  // 10
      repeat10 = false;
      double R = RNDM();
      NCOU++;
      if (NCOU >= 11) {
        goto20 = true;
        break;
      }
      if (R < F / (F + 2.71828)) {  // goto30
        Y = 1. - std::log(RNDM() + 1.e-7);
        if (RNDM() > std::pow(Y, F - 1.)) {
          repeat10 = true;
          continue;
        } else {
          goto20 = false;
          break;
        }
      }
      double YYY = std::log(RNDM() + 1.e-7) / F;
      if (std::abs(YYY) > 50.) {
        goto20 = true;
        break;
      }
      Y = std::exp(YYY);
      if (std::log(RNDM() + 1.e-7) > -Y && goto20 == false) {
        repeat10 = true;
        continue;
      }
    } while (repeat10);

    if (goto20) {  // equals goto20 statement
      Y = 0.;
    } else {
      if (n == 0) return Y;
    }
  }

  double Z = 1.;
  for (int i = 0; i < n; ++i) {
    Z *= RNDM();
  }
  Y -= std::log(Z + 1.e-7);
  return Y;
}

static bool lund_frag_isInitialized = false;
void sophia_interface::lund_frag(double SQS) {
  // interface to Lund/Jetset fragmentation (R.E. 08/98)
  int KC = 0;
  if (lund_frag_isInitialized == false) {
    // no title page
    MSTU[11] = 0;

    // define some particles as stable (in particular pions and kaons)
    MSTJ[21] = 2;

    // in addition pi0 stable
    int KC = LUCOMP(111);
    MDCY[0][KC - 1] = 0;

    // switch popcorn effect off
    MSTJ[11] = 1;

    // suppress all warning and error messages
    MSTU[21] = 0;
    MSTU[24] = 0;

    lund_frag_isInitialized = true;
  }

  // set energy-dependent parameters
  if (SQS < 2.) {
    PARJ[35] = 0.1;
  } else if (SQS < 4.) {
    PARJ[35] = 0.7 * (SQS - 2.) / 2. + 0.1;
  } else {
    PARJ[35] = 0.8;
  }

  // fragment string configuration
  int II = MSTU[20];
  MSTU[20] = 1;

  LUEXEC();

  MSTU[20] = II;

  // event accepted?
  if (MSTU[23] != 0) {
    np = -1;
    return;
  }

  LUEDIT(1);

  np = KLU(0, 1);

  return;
}

void sophia_interface::lund_put(int I, int IFL, double PX, double PY, double PZ, double EE) {
  // store initial configuration into Lund common block (R.E. 08/98)
  int Il = 0;
  switch (IFL) {
    case 1:
      Il = 2;
      break;
    case -1:
      Il = -2;
      break;
    case 2:
      Il = 1;
      break;
    case -2:
      Il = -1;
      break;
    case 11:
      Il = 2203;
      break;
    case 12:
      Il = 2101;
      break;
    case 21:
      Il = 2103;
      break;
    case 22:
      Il = 1103;
      break;
    default:
      std::cout << "flavour code: " << IFL << std::endl;
      throw std::runtime_error("lund_put: unknown flavor code");
  }

  P[0][I - 1] = PX;
  P[1][I - 1] = PY;
  P[2][I - 1] = PZ;
  P[3][I - 1] = EE;
  P[4][I - 1] = (EE - PZ) * (EE + PZ) - PX * PX -
                PY * PY;  // there are floating point precision problems compared to FORTRAN. in
                          // that case abs() can be added here

  K[0][I - 1] = 1;
  K[1][I - 1] = Il;
  K[2][I - 1] = 0;
  K[3][I - 1] = 0;
  K[4][I - 1] = 0;

  N = I;

  return;
}

lund_get_output sophia_interface::lund_get(int I) {
  // read final states from Lund common block (R.E. 08/98)
  lund_get_output lgo;

  lgo.PX = PLU(I, 1);
  lgo.PY = PLU(I, 2);
  lgo.PZ = PLU(I, 3);
  lgo.EE = PLU(I, 4);
  lgo.XM = PLU(I, 5);

  int Il = KLU(I, 8);
  // convert particle ID
  lgo.IFL = ICON_PDG_SIB(Il);

  return lgo;
}

int sophia_interface::ICON_PDG_SIB(int ID) {
  // convert PDG particle codes to SIBYLL particle codes (R.E. 09/97)
  int ITABLE[49] = {22,   -11,  11,   -13,  13,   111,  211,  -211, 321,       -321,
                    130,  310,  2212, 2112, 12,   -12,  14,   -14,  -99999999, -99999999,
                    311,  -311, 221,  331,  213,  -213, 113,  323,  -323,      313,
                    -313, 223,  333,  3222, 3212, 3112, 3322, 3312, 3122,      2224,
                    2214, 2114, 1114, 3224, 3214, 3114, 3324, 3314, 3334};

  int IDPDG = ID;
  int IDA = std::abs(ID);
  int IS = IDPDG;
  int IC = 1;
  if (IDA > 1000) {
    IS = IDA;
    IC = (IDPDG < 0) ? -1 : 1;
  }

  for (int i = 0; i < 49; ++i) {
    if (IS == ITABLE[i]) {
      return (i + 1) * IC;
    }
  }

  if (IDPDG == 80000) {
    return 13;
  } else {
    std::cout << "ICON_PDG_DTU: no particle found for " << IDPDG << std::endl;
    return 0;
  }
}

double sophia_interface::PO_XLAM(double X, double Y, double Z) {
  //    auxiliary function for two/three particle decay mode
  //    (standard LAMBDA**(1/2) function)
  //    (taken from PHOJET 1.12, R.E. 08/98)
  double YZ = Y - Z;
  double XLAM = X * X - 2. * X * (Y + Z) + YZ * YZ;
  if (XLAM < 0.) XLAM = -XLAM;
  return std::sqrt(XLAM);
}

double sophia_interface::Pl(double x, double xth, double xmax, double alpha) {
  if (xth > x) return 0.;
  const double a = alpha * xmax / xth;
  const double prod1 = std::pow((x - xth) / (xmax - xth), a - alpha);
  const double prod2 = std::pow(x / xmax, -a);
  return prod1 * prod2;
}

double sophia_interface::Ef(double x, double th, double w) {
  double wth = w + th;
  if (x <= th) {
    return 0.;
  } else if (x > th && x < wth) {
    return (x - th) / w;
  } else if (x >= wth) {
    return 1.;
  } else {
    throw std::runtime_error("error in function Ef");
  }
}

double sophia_interface::breitwigner(double sigma_0, double Gamma, double DMM, double eps_prime) {
  const double pm = 0.93827;  // Gev/c^2
  const double s = pm * pm + 2. * pm * eps_prime;
  const double gam2s = Gamma * Gamma * s;
  return sigma_0 * (s / eps_prime / eps_prime) * gam2s /
         ((s - DMM * DMM) * (s - DMM * DMM) + gam2s);
}

double sophia_interface::RNDM() {
  // This is the RNG called by all other non-JETSET routines.
  // The same as RLU from JETSET except for another seed.
  bool isCalledByRNDM = true;
  return RLU(isCalledByRNDM);
}

void sophia_interface::LUEXEC() {
  // Purpose: to administrate the fragmentation and decay chain.
  double PS[6][2] = {0.};

  // Initialize and reset.
  MSTU[23] = 0;
  if (MSTU[11] >= 1) LUERRM(-1, "LULIST(0), Method removed.");
  MSTU[30]++;
  MSTU[0] = 0;
  MSTU[1] = 0;
  MSTU[2] = 0;
  if (MSTU[16] <= 0) MSTU[89] = 0;
  int MCONS = 1;

  // Sum up momentum, energy and charge for starting entries.
  int NSAV = N;
  for (int I = 0; I < N; ++I) {
    if (K[0][I] <= 0 || K[0][I] > 10) continue;
    for (int J = 0; J < 4; ++J) {
      PS[J][0] += P[J][I];
    }
    PS[5][0] += LUCHGE(K[1][I]);
  }
  PARU[20] = PS[3][0];

  // Prepare system for subsequent fragmentation/decay.
  LUPREP(0);

  // Loop through jet fragmentation and particle decays.
  int MBE = 0;
  bool repeat140 = false;
  do {  // 140
    repeat140 = false;
    MBE++;
    int IP = 0;

    bool repeat150 = false;
    do {  // 150
      repeat150 = false;
      IP++;
      int KC = 0;
      if (K[0][IP - 1] > 0 && K[0][IP - 1] <= 10) KC = LUCOMP(K[1][IP - 1]);
      if (KC == 0) {
        // do nothing
        // Particle decay if unstable and allowed. Save long-lived particle
        // decays until second pass after Bose-Einstein effects.
      } else if (KCHG[1][KC - 1] == 0) {
        if (MSTJ[20] >= 1 && MDCY[0][KC - 1] >= 1 &&
            (MSTJ[50] <= 0 || MBE == 2 || PMAS[1][KC - 1] >= PARJ[90] ||
             std::abs(K[1][IP - 1]) == 311)) {
          LUDECY(IP);
        }

        // Decay products may develop a shower.
        if (MSTJ[91] > 0) {
          int IP1 = MSTJ[91];
          double QMAX = std::sqrt(
              std::max(0., (P[3][IP1 - 1] + P[3][IP1]) * (P[3][IP1 - 1] + P[3][IP1]) -
                               (P[0][IP1 - 1] + P[0][IP1]) * (P[0][IP1 - 1] + P[0][IP1]) -
                               (P[1][IP1 - 1] + P[1][IP1]) * (P[1][IP1 - 1] + P[1][IP1]) -
                               (P[2][IP1 - 1] + P[2][IP1]) * (P[2][IP1 - 1] + P[2][IP1])));
          LUSHOW(IP1, IP1 + 1, QMAX);
          LUPREP(IP1);
          MSTJ[91] = 0;
        } else if (MSTJ[91] < 0) {
          int IP1 = -MSTJ[91];
          LUSHOW(IP1, -3, P[4][IP - 1]);
          LUPREP(IP1);
          MSTJ[91] = 0;
        }

        // Jet fragmentation: string or independent fragmentation.
      } else if (K[0][IP - 1] == 1 || K[0][IP - 1] == 2) {
        int MFRAG = MSTJ[0];
        if (MFRAG >= 1 && K[0][IP - 1] == 1) MFRAG = 2;
        if (MSTJ[20] >= 2 && K[0][IP - 1] == 2 && N > IP) {
          if (K[0][IP] == 1 && K[2][IP] == K[2][IP - 1] && K[2][IP - 1] > 0 && K[2][IP - 1] < IP) {
            if (KCHG[1][LUCOMP(K[1][K[2][IP - 1] - 1]) - 1] == 0) MFRAG = std::min(1, MFRAG);
          }
        }

        if (MFRAG == 1) LUSTRF(IP);
        if (MFRAG == 2) LUINDF(IP);

        if (MFRAG == 2 && K[0][IP - 1] == 1) MCONS = 0;
        if (MFRAG == 2 && (MSTJ[2] <= 0 || MSTJ[2] % 5 == 0)) MCONS = 0;
      }

      // Loop back if enough space left in LUJETS and no error abort.
      if (MSTU[23] != 0 && MSTU[20] >= 2) {
        // do nothing
      } else if (IP < N && N < MSTU[3] - 20 - MSTU[31]) {
        repeat150 = true;
        continue;
      } else if (IP < N) {
        LUERRM(11, "LUEXEC: no more memory left in LUJETS");
      }
    } while (repeat150);  // 150

    // Include simple Bose-Einstein effect parametrization if desired.
    if (MBE == 1 && MSTJ[50] >= 1) {
      LUBOEI(NSAV);
      repeat140 = true;
      continue;
    }
  } while (repeat140);  // 140

  // Check that momentum, energy and charge were conserved.
  for (int I = 0; I < N; ++I) {
    if (K[0][I] <= 0 || K[0][I] > 10) continue;
    for (int J = 0; J < 4; ++J) {
      PS[J][1] += P[J][I];
    }
    PS[5][1] += LUCHGE(K[1][I]);
  }
  double PDEV = (std::abs(PS[0][1] - PS[0][0]) + std::abs(PS[1][1] - PS[1][0]) +
                 std::abs(PS[2][1] - PS[2][0]) + std::abs(PS[3][1] - PS[3][0])) /
                (1. + std::abs(PS[3][1]) + std::abs(PS[3][0]));
  if (MCONS == 1 && PDEV > PARU[10]) LUERRM(15, "LUEXEC: four-momentum was not conserved");
  if (MCONS == 1 && std::abs(PS[5][1] - PS[5][0]) > 0.1)
    LUERRM(15, "LUEXEC: charge was not conserved");
  return;
}

void sophia_interface::LUDECY(int IP) {
  // Purpose: to handle the decay of unstable particles.
  double VDCY[4] = {0.};
  int KFLO[4] = {0};
  int KFL1[4] = {0};
  double PV[5][10] = {0.};
  double RORD[10] = {0.};
  double UE[3] = {0.};
  double BE[3] = {0.};
  double PTAU[4] = {0.};
  double PCMTAU[4] = {0.};
  double DBETAU[3] = {0.};
  double WTCOR[10] = {2., 5., 15., 60., 250., 1500., 1.2e4, 1.2e5, 150., 16.};

  // Functions: momentum in two-particle decays, four-product and
  // matrix element times phase space in weak decays.
  std::function<double(double, double, double)> PAWT = [&](double A, double B, double C) {
    return std::sqrt((A * A - (B + C) * (B + C)) * (A * A - (B - C) * (B - C))) / (2. * A);
  };
  std::function<double(int, int)> FOUR = [&](int I, int J) {
    return P[3][I - 1] * P[3][J - 1] - P[0][I - 1] * P[0][J - 1] - P[1][I - 1] * P[1][J - 1] -
           P[2][I - 1] * P[2][J - 1];
  };
  std::function<double(double, double)> HMEPS = [&](int HA, int HRQ) {
    return ((1. - HRQ - HA) * (1. - HRQ - HA) + 3. * HA * (1. + HRQ - HA)) *
           std::sqrt((1. - HRQ - HA) * (1. - HRQ - HA) - 4. * HRQ * HA);
  };

  // declaration of values which in FORTRAN have not required initialization
  int MMAT = 0;
  int MBST = 0;
  int ND = 0;
  int MMIX = 0;
  int NP = 0;
  int NQ = 0;
  double PS = 0.;
  double PSQ = 0.;
  int KQP = 0;

  // Initial values.
  int NTRY = 0;
  int NSAV = N;
  int KFA = std::abs(K[1][IP - 1]);
  int KFS = (K[1][IP - 1] < 0) ? -1 : 1;
  int KC = LUCOMP(KFA);
  MSTJ[91] = 0;

  // Choose lifetime and determine decay vertex.
  if (K[0][IP - 1] == 5) {
    V[4][IP - 1] = 0.;
  } else if (K[0][IP - 1] != 4) {
    V[4][IP - 1] = -PMAS[3][KC - 1] * std::log(RLU());
  }
  for (int J = 0; J < 4; ++J) {
    VDCY[J] = V[J][IP - 1] + V[4][IP - 1] * P[J][IP - 1] / P[4][IP - 1];
  }

  // Determine whether decay allowed or not.
  int MOUT = 0;
  if (MSTJ[21] == 2) {
    if (PMAS[3][KC - 1] > PARJ[70]) MOUT = 1;
  } else if (MSTJ[21] == 3) {
    if (VDCY[0] * VDCY[0] + VDCY[1] * VDCY[1] + VDCY[2] * VDCY[2] > PARJ[71] * PARJ[71]) MOUT = 1;
  } else if (MSTJ[21] == 4) {
    if (VDCY[0] * VDCY[0] + VDCY[1] * VDCY[1] > PARJ[72] * PARJ[72]) MOUT = 1;
    if (std::abs(VDCY[2]) > PARJ[73]) MOUT = 1;
  }
  if (MOUT == 1 && K[0][IP - 1] != 5) {
    K[0][IP - 1] = 4;
    return;
  }

  // Interface to external tau decay library (for tau polarization).
  if (KFA == 15 && MSTJ[27] >= 1) {
    // Starting values for pointers and momenta.
    int ITAU = IP;
    for (int J = 0; J < 4; ++J) {
      PTAU[J] = P[J][ITAU - 1];
      PCMTAU[J] = P[J][ITAU - 1];
    }

    // Iterate to find position and code of mother of tau.
    int IMTAU = ITAU;
    int KFORIG = 0;
    int IORIG = 0;
    bool repeat120 = false;
    do {
      repeat120 = false;
      IMTAU = K[2][IMTAU - 1];

      if (IMTAU == 0) {
        // If no known origin then impossible to do anything further.
        KFORIG = 0;
        IORIG = 0;
      } else if (K[1][IMTAU - 1] == K[1][ITAU - 1]) {
        // If tau -> tau + gamma then add gamma energy and loop.
        if (K[1][K[3][IMTAU - 1] - 1] == 22) {
          for (int J = 0; J < 4; ++J) {
            PCMTAU[J] += P[J][K[3][IMTAU - 1] - 1];
          }
        } else if (K[1][K[4][IMTAU - 1] - 1] == 22) {
          for (int J = 0; J < 4; ++J) {
            PCMTAU[J] += P[J][K[4][IMTAU - 1] - 1];
          }
        }
        repeat120 = true;
        continue;

      } else if (std::abs(K[1][IMTAU - 1]) > 100) {
        // If coming from weak decay of hadron then W is not stored in record,
        // but can be reconstructed by adding neutrino momentum.
        int sgn = (K[1][ITAU - 1] < 0) ? -1 : 1;
        KFORIG = -sgn * 24;
        IORIG = 0;
        for (int II = K[3][IMTAU - 1]; II < K[4][IMTAU - 1] + 1; ++II) {
          int sgn = (K[1][ITAU - 1]) ? -1 : 1;
          if (K[1][II - 1] * sgn == -16) {
            for (int J = 0; J < 4; ++J) {
              PCMTAU[J] += P[J][II - 1];
            }
          }
        }
      } else {
        // If coming from resonance decay then find latest copy of this
        // resonance (may not completely agree).
        KFORIG = K[1][IMTAU - 1];
        IORIG = IMTAU;
        for (int II = IMTAU + 1; II < IP; ++II) {
          if (K[1][II - 1] == KFORIG && K[2][II - 1] == IORIG &&
              std::abs(P[4][II - 1] - P[4][IORIG - 1]) < 1e-5 * P[4][IORIG - 1])
            IORIG = II;
        }
        for (int J = 0; J < 4; ++J) {
          PCMTAU[J] = P[J][IORIG - 1];
        }
      }
    } while (repeat120);

    // Boost tau to rest frame of production process (where known)
    // and rotate it to sit along  + z axis.
    for (int J = 0; J < 3; ++J) {
      DBETAU[J] = PCMTAU[J] / PCMTAU[3];
    }
    if (KFORIG != 0) LUDBRB(ITAU, ITAU, 0., 0., -DBETAU[0], -DBETAU[1], -DBETAU[2]);
    double PHITAU = ULANGL(P[0][ITAU - 1], P[1][ITAU - 1]);
    LUDBRB(ITAU, ITAU, 0., -PHITAU, 0., 0., 0.);
    double THETAU = ULANGL(P[2][ITAU - 1], P[0][ITAU - 1]);
    LUDBRB(ITAU, ITAU, -THETAU, 0., 0., 0., 0.);

    // Call tau decay routine (if meaningful) and fill extra info.
    if (KFORIG != 0 || MSTJ[27] == 2) {
      int NDECAY = ITAU + IORIG + KFORIG;
      std::cout
          << "LUDECY: LUTAUD: if this message occurs, JETSET would have been terminated, failing."
          << std::endl;
      for (int II = NSAV; II < NSAV + NDECAY; ++II) {
        K[0][II] = 1;
        K[2][II] = IP;
        K[3][II] = 0;
        K[4][II] = 0;
      }
      N = NSAV + NDECAY;
    }

    // Boost back decay tau and decay products.
    for (int J = 0; J < 4; ++J) {
      P[J][ITAU - 1] = PTAU[J];
    }
    if (KFORIG != 0 || MSTJ[27] == 2) {
      LUDBRB(NSAV + 1, N, THETAU, PHITAU, 0., 0., 0.);
      if (KFORIG != 0) LUDBRB(NSAV + 1, N, 0., 0., DBETAU[0], DBETAU[1], DBETAU[2]);

      // Skip past ordinary tau decay treatment.
      MMAT = 0;
      MBST = 0;
      ND = 0;

      // goto 660 statement
      N += ND;
      if (MBST == 1) {
        for (int J = 0; J < 3; ++J) {
          BE[J] = P[J][IP - 1] / P[3][IP - 1];
        }
        double GA = P[3][IP - 1] / P[4][IP - 1];
        for (int Ii = NSAV; Ii < N; ++Ii) {
          double BEP = BE[0] * P[0][Ii] + BE[1] * P[1][Ii] + BE[2] * P[2][Ii];
          for (int J = 0; J < 3; ++J) {
            P[J][Ii] += GA * (GA * BEP / (1. + GA) + P[3][Ii]) * BE[J];
          }
          P[3][Ii] = GA * (P[3][Ii] + BEP);
        }
      }

      // Fill in position of decay vertex.
      for (int Ii = NSAV; Ii < N; ++Ii) {
        for (int J = 0; J < 4; ++J) {
          V[J][Ii] = VDCY[J];
        }
        V[4][Ii] = 0.;
      }

      LUDECY_setupPartonShowerEvolution(IP, NSAV, MMAT, ND, MMIX);
      return;
    }
  }

  // B-B~ mixing: flip sign of meson appropriately.
  MMIX = 0;
  if ((KFA == 511 || KFA == 531) && MSTJ[25] >= 1) {
    double XBBMIX = PARJ[75];
    if (KFA == 531) XBBMIX = PARJ[76];
    if (std::pow(std::sin(0.5 * XBBMIX * V[4][IP - 1] / PMAS[3][KC - 1]), 2) > RLU()) MMIX = 1;
    if (MMIX == 1) KFS = -KFS;
  }

  // Check existence of decay channels. Particle / antiparticle rules.
  int KCA = KC;
  if (MDCY[1][KC - 1] > 0) {
    int MDMDCY = MDME[1][MDCY[1][KC - 1] - 1];
    if (MDMDCY > 80 && MDMDCY <= 90) KCA = MDMDCY;
  }
  if (MDCY[1][KCA - 1] <= 0 || MDCY[2][KCA - 1] <= 0) {
    LUERRM(9, "(LUDECY:) no decay channel defined");
    return;
  }
  int KFSP = 0;
  int KFSN = 0;
  if ((KFA / 1000) % 10 == 0 && (KCA == 85 || KCA == 87)) KFS = -KFS;
  if (KCHG[2][KC - 1] == 0) {
    KFSP = 1;
    KFSN = 0;
    if (RLU() > 0.5) KFS = -KFS;
  } else if (KFS > 0) {
    KFSP = 1;
    KFSN = 0;
  } else {
    KFSP = 0;
    KFSN = 1;
  }

  // Sum branching ratios of allowed decay channels.
  int NOPE = 0;
  double BRSU = 0.;
  for (int IDL = MDCY[1][KCA - 1]; IDL < MDCY[1][KCA - 1] + MDCY[2][KCA - 1]; ++IDL) {
    if (MDME[0][IDL - 1] != 1 && KFSP * MDME[0][IDL - 1] != 2 && KFSN * MDME[0][IDL - 1] != 3)
      continue;
    if (MDME[1][IDL - 1] > 100) continue;
    NOPE++;
    BRSU += BRAT[IDL - 1];
  }
  if (NOPE == 0) {
    LUERRM(2, "(LUDECY:) all decay channels closed by user");
    return;
  }

  // Select decay channel among allowed ones.
  int IDC = 0;
  int MREM = 0;

  // from loop 240/260/290
  double HW = 0.;
  double HRQ = 0.;

  // from loop 420/490
  double PMES = 0.;
  double PMST = 0.;
  double WT = 0.;

  bool goto240 = true;
  bool goto260 = false;
  bool goto290 = false;
  do {  // 240, 260, 290
    if (goto240 && goto260 || goto240 && goto290 || goto260 && goto290)
      throw std::runtime_error("LUDECY: misguided goto switch arrangement");

    if (goto240) {
      goto240 = false;

      double RBR = BRSU * RLU();
      int IDL = MDCY[1][KCA - 1] - 1;

      bool repeat250 = false;
      do {  // 250
        repeat250 = false;
        IDL++;
        if (MDME[0][IDL - 1] != 1 && KFSP * MDME[0][IDL - 1] != 2 && KFSN * MDME[0][IDL - 1] != 3) {
          if (IDL < MDCY[1][KCA - 1] + MDCY[2][KCA - 1] - 1) {
            repeat250 = true;
            continue;
          }
        } else if (MDME[1][IDL - 1] > 100) {
          if (IDL < MDCY[1][KCA - 1] + MDCY[2][KCA - 1] - 1) {
            repeat250 = true;
            continue;
          }
        } else {
          IDC = IDL;
          RBR -= BRAT[IDL - 1];
          if (IDL < MDCY[1][KCA - 1] + MDCY[2][KCA - 1] - 1 && RBR > 0.) {
            repeat250 = true;
            continue;
          }
        }
      } while (repeat250);

      // Start readout of decay channel: matrix element, reset counters.
      MMAT = MDME[1][IDC - 1];

      goto260 = true;
    }

    if (goto260) {
      goto260 = false;

      NTRY++;
      if (NTRY > 1000) {
        LUERRM(14, "(LUDECY:) caught in infinite loop");
        if (MSTU[20] >= 1) return;
      }
      int I = N;
      NP = 0;
      NQ = 0;
      MBST = 0;
      if (MMAT >= 11 && MMAT != 46 && P[3][IP - 1] > 20. * P[4][IP - 1]) MBST = 1;
      for (int J = 0; J < 4; ++J) {
        PV[J][0] = 0.;
        if (MBST == 0) PV[J][0] = P[J][IP - 1];
      }
      if (MBST == 1) PV[3][0] = P[4][IP - 1];
      PV[4][0] = P[4][IP - 1];
      PS = 0.;
      PSQ = 0.;
      MREM = 0;
      int MHADDY = 0;
      if (KFA > 80) MHADDY = 1;

      // Read out decay products. Convert to standard flavour code.
      int JTMAX = 5;
      if (MDME[1][IDC] == 101) JTMAX = 10;
      int KP = 0;
      double KFP = 0.;
      for (int JT = 1; JT < JTMAX + 1; ++JT) {
        if (JT <= 5) KP = KFDP[JT - 1][IDC - 1];
        if (JT >= 6) KP = KFDP[JT - 6][IDC];
        if (KP == 0) continue;
        int KPA = std::abs(KP);
        int KCP = LUCOMP(KPA);
        if (KPA > 80) MHADDY = 1;
        if (KCHG[2][KCP - 1] == 0 && KPA != 81 && KPA != 82) {
          KFP = KP;
        } else if (KPA != 81 && KPA != 82) {
          KFP = KFS * KP;
        } else if (KPA == 81 && (KFA / 1000) % 10 == 0) {
          KFP = -KFS * ((KFA / 10) % 10);
        } else if (KPA == 81 && (KFA / 100) % 10 >= (KFA / 10) % 10) {
          KFP = KFS * (100 * ((KFA / 10) % 100) + 3);
        } else if (KPA == 81) {
          KFP = KFS * (1000 * ((KFA / 10) % 10) + 100 * ((KFA / 100) % 10) + 1);
        } else if (KP == 82) {
          std::vector<int> LUKFDI_output =
              LUKFDI(-KFS * static_cast<int>(1. + (2. + PARJ[1]) * RLU()), 0);
          KFP = LUKFDI_output[0];
          if (KFP == 0) {
            goto240 = false;
            goto260 = true;
            goto290 = false;
            break;
          }
          MSTJ[92] = 1;
          if (PV[4][0] < PARJ[31] + 2. * ULMASS(KFP)) {
            goto240 = false;
            goto260 = true;
            goto290 = false;
            break;
          }
        } else if (KP == -82) {
          KFP = -KFP;
          if (std::abs(KFP) > 10) {
            int sgn = (KFP < 0) ? -1 : 1;
            KFP += 10000 * sgn;
          }
        }
        if (KPA == 81 || KPA == 82) KCP = LUCOMP(KFP);

        // Add decay product to event record or to quark flavour list.
        int KFPA = std::abs(KFP);
        KQP = KCHG[1][KCP - 1];
        if (MMAT >= 11 && MMAT <= 30 && KQP != 0) {
          NQ++;
          KFLO[NQ - 1] = KFP;
          MSTJ[92] = 2;
          PSQ += ULMASS(KFLO[NQ - 1]);
        } else if ((MMAT == 42 || MMAT == 43 || MMAT == 48) && NP == 3 && NQ % 2 == 1) {
          NQ--;
          PS -= P[4][I - 1];
          K[0][I - 1] = 1;
          int KFI = K[1][I - 1];
          std::vector<int> LUKFDI_output = LUKFDI(KFP, KFI);
          K[1][I - 1] = LUKFDI_output[1];
          if (K[1][I - 1] == 0) {
            goto240 = false;
            goto260 = true;
            goto290 = false;
            break;
          }
          MSTJ[92] = 1;
          P[4][I - 1] = ULMASS(K[1][I - 1]);
          PS += P[4][I - 1];
        } else {
          I++;
          NP++;
          if (MMAT != 33 && KQP != 0) NQ++;
          if (MMAT == 33 && KQP != 0 && KQP != 2) NQ++;
          K[0][I - 1] = 1 + NQ % 2;
          if (MMAT == 4 && JT <= 2 && KFP == 21) K[0][I - 1] = 2;
          if (MMAT == 4 && JT == 3) K[0][I - 1] = 1;
          K[1][I - 1] = KFP;
          K[2][I - 1] = IP;
          K[3][I - 1] = 0;
          K[4][I - 1] = 0;
          P[4][I - 1] = ULMASS(KFP);
          if (MMAT == 45 && KFPA == 89) P[4][I - 1] = PARJ[31];
          PS += P[4][I - 1];
        }
      }
      if (goto240 || goto260 || goto290) continue;

      // Check masses for resonance decays.
      if (MHADDY == 0) {
        if (PS + PARJ[63] > PV[4][0]) {
          goto240 = true;
          goto260 = false;
          goto290 = false;
          continue;
        }
      }
    }

    // 290
    goto290 = false;

    // Choose decay multiplicity in phase space model.
    double PQT = 0.;
    if (MMAT >= 11 && MMAT <= 30) {
      double PSP = PS;
      double CNDE = PARJ[60] * std::log(std::max((PV[4][0] - PS - PSQ) / PARJ[61], 1.1));
      if (MMAT == 12) CNDE += PARJ[62];

      bool repeat300 = false;
      do {  // 300
        repeat300 = false;
        NTRY++;
        if (NTRY > 1000) {
          LUERRM(14, "(LUDECY:) caught in infinite loop");
          if (MSTU[20] >= 1) return;
        }
        if (MMAT <= 20) {
          double GAUSS =
              std::sqrt(-2. * CNDE * std::log(std::max(1e-10, RLU()))) * std::sin(PARU[1] * RLU());
          ND = 0.5 + 0.5 * NP + 0.25 * NQ + CNDE + GAUSS;
          if (ND < NP + NQ / 2 || ND < 2 || ND > 10) {
            repeat300 = true;
            continue;
          }
          if (MMAT == 13 && ND == 2) {
            repeat300 = true;
            continue;
          }
          if (MMAT == 14 && ND <= 3) {
            repeat300 = true;
            continue;
          }
          if (MMAT == 15 && ND <= 4) {
            repeat300 = true;
            continue;
          }
        } else {
          ND = MMAT - 20;
        }

        // Form hadrons from flavour content.
        for (int JT = 0; JT < 4; ++JT) {
          KFL1[JT] = KFLO[JT];
        }
        int JT = 0;
        if (ND != NP + NQ / 2) {  // skip condition 330
          for (int Ii = N + NP; Ii < N + ND - NQ / 2; ++Ii) {
            JT = 1 + static_cast<int>((NQ - 1) * RLU());
            std::vector<int> LUKFDI_output = LUKFDI(KFL1[JT - 1], 0);
            int KFL2 = LUKFDI_output[0];
            K[1][Ii] = LUKFDI_output[1];
            if (K[1][Ii] == 0) {
              repeat300 = true;
              break;
            }
            KFL1[JT - 1] = -KFL2;
          }
          if (repeat300) continue;
        }

        JT = 2;
        int JT2 = 3;
        int JT3 = 4;
        if (NQ == 4 && RLU() < PARJ[65]) JT = 4;
        int sgn1 = (KFL1[0] * (10 - std::abs(KFL1[0])) < 0) ? -1 : 1;
        int sgn2 = (KFL1[JT - 1] * (10 - std::abs(KFL1[JT - 1])) < 0) ? -1 : 1;
        if (JT == 4 && sgn1 * sgn2 > 0) JT = 3;
        if (JT == 3) JT2 = 2;
        if (JT == 4) JT3 = 2;
        std::vector<int> LUKFDI_output = LUKFDI(KFL1[0], KFL1[JT - 1]);
        K[1][N + ND - NQ / 2] = LUKFDI_output[1];
        if (K[1][N + ND - NQ / 2] == 0) {
          repeat300 = true;
          continue;
        }
        if (NQ == 4) {
          std::vector<int> LUKFDI_output = LUKFDI(KFL1[JT2 - 1], KFL1[JT3 - 1]);
          K[1][N + ND - 1] = LUKFDI_output[1];
        }
        if (NQ == 4 && K[1][N + ND - 1] == 0) {
          repeat300 = true;
          continue;
        }

        // Check that sum of decay product masses not too large.
        PS = PSP;
        for (int Ii = N + NP; Ii < N + ND; ++Ii) {
          K[0][Ii] = 1;
          K[2][Ii] = IP;
          K[3][Ii] = 0;
          K[4][Ii] = 0;
          P[4][Ii] = ULMASS(K[1][Ii]);
          PS += P[4][Ii];
        }
        if (PS + PARJ[63] > PV[4][0]) {
          repeat300 = true;
          continue;
        }
      } while (repeat300);

      // Rescale energy to subtract off spectator quark mass.
    } else if ((MMAT == 31 || MMAT == 33 || MMAT == 44 || MMAT == 45) && NP >= 3) {
      PS -= P[4][N + NP - 1];
      PQT = (P[4][N + NP - 1] + PARJ[64]) / PV[4][0];
      for (int J = 0; J < 5; ++J) {
        P[J][N + NP - 1] = PQT * PV[J][0];
        PV[J][0] *= (1. - PQT);
      }
      if (PS + PARJ[63] > PV[4][0]) {
        goto240 = false;
        goto260 = true;
        goto290 = false;
        continue;
      }
      ND = NP - 1;
      MREM = 1;

      // Phase space factors imposed in W decay.
    } else if (MMAT == 46) {
      MSTJ[92] = 1;
      double PSMC = ULMASS(K[1][N]);
      MSTJ[92] = 1;
      PSMC += ULMASS(K[1][N + 1]);
      if (std::max(PS, PSMC) + PARJ[31] > PV[4][0]) {
        goto240 = true;
        goto260 = false;
        goto290 = false;
        continue;
      }
      double HR1 = (P[4][N] / PV[4][0]) * (P[4][N] / PV[4][0]);
      double HR2 = (P[4][N + 1] / PV[4][0]) * (P[4][N + 1] / PV[4][0]);
      if ((1. - HR1 - HR2) * (2. + HR1 + HR2) *
              std::sqrt((1. - HR1 - HR2) * (1. - HR1 - HR2) - 4. * HR1 * HR2) <
          2. * RLU()) {
        goto240 = true;
        goto260 = false;
        goto290 = false;
        continue;
      }
      ND = NP;

      // Fully specified final state: check mass broadening effects.
    } else {
      if (NP >= 2 && PS + PARJ[63] > PV[4][0]) {
        goto240 = false;
        goto260 = true;
        goto290 = false;
        continue;
      }
      ND = NP;
    }

    // Select W mass in decay Q  - > W  +  q, without W propagator.
    if (MMAT == 45 && MSTJ[24] <= 0) {
      double HLQ = (PARJ[31] / PV[4][0]) * (PARJ[31] / PV[4][0]);
      double HUQ = std::pow(1. - (P[4][N + 1] + PARJ[63]) / PV[4][0], 2);
      HRQ = std::pow(P[4][N + 1] / PV[4][0], 2);
      do {
        HW = HLQ + RLU() * (HUQ - HLQ);
      } while (HMEPS(HW, HRQ) < RLU());
      P[4][N] = PV[4][0] * std::sqrt(HW);

      // Ditto, including W propagator. Divide mass range into three regions.
    } else if (MMAT == 45) {
      double HQW = std::pow(PV[4][0] / PMAS[0][23], 2);
      double HLW = std::pow(PARJ[31] / PMAS[0][23], 2);
      double HUW = std::pow((PV[4][0] - P[4][N + 1] - PARJ[63]) / PMAS[0][23], 2);
      HRQ = std::pow(P[4][N + 1] / PV[4][0], 2);
      double HG = PMAS[1][23] / PMAS[0][23];
      double HATL = std::atan((HLW - 1.) / HG);
      double HM = std::min(1., HUW - 0.001);
      double HMV1 = HMEPS(HM / HQW, HRQ) / ((HM - 1.) * (HM - 1.) + HG * HG);

      double HMV2 = 0.;
      bool repeat370 = false;
      do {  // 370
        repeat370 = false;
        HM -= HG;
        HMV2 = HMEPS(HM / HQW, HRQ) / ((HM - 1.) * (HM - 1.) + HG * HG);
        if (HMV2 > HMV1 && HM - HG > HLW) {
          HMV1 = HMV2;
          repeat370 = true;
          continue;
        }
      } while (repeat370);

      double HMV = std::min(2. * HMV1, HMEPS(HM / HQW, HRQ) / (HG * HG));
      double HM1 = 1. - std::sqrt(1. / HMV - HG * HG);
      if (HM1 > HLW && HM1 < HM) {
        HM = HM1;
      } else if (HMV2 <= HMV1) {
        HM = std::max(HLW, HM - std::min(0.1, 1. - HM));
      }
      double HATM = std::atan((HM - 1.) / HG);
      double HWT1 = (HATM - HATL) / HG;
      double HWT2 = HMV * (std::min(1., HUW) - HM);
      double HWT3 = 0.;
      double HATU = 0.;
      double HMP1 = 0.;
      if (HUW > 1.) {
        HATU = std::atan((HUW - 1.) / HG);
        HMP1 = HMEPS(1. / HQW, HRQ);
        HWT3 = HMP1 * HATU / HG;
      }

      // Select mass region and W mass there. Accept according to weight.
      double HACC = 0.;
      do {
        double HREG = RLU() * (HWT1 + HWT2 + HWT3);
        if (HREG <= HWT1) {
          HW = 1. + HG * std::tan(HATL + RLU() * (HATM - HATL));
          HACC = HMEPS(HW / HQW, HRQ);
        } else if (HREG <= HWT1 + HWT2) {
          HW = HM + RLU() * (std::min(1., HUW) - HM);
          HACC = HMEPS(HW / HQW, HRQ) / ((HW - 1.) * (HW - 1.) + HG * HG) / HMV;
        } else {
          HW = 1. + HG * std::tan(RLU() * HATU);
          HACC = HMEPS(HW / HQW, HRQ) / HMP1;
        }
      } while (HACC < RLU());
      P[4][N] = PMAS[0][23] * std::sqrt(HW);
    }

    // Determine position of grandmother, number of sisters, Q  - > W sign.
    int NM = 0;
    int KFAS = 0;
    int MSGN = 0;
    int IM = 0;
    if (MMAT == 3 || MMAT == 46) {
      IM = K[2][IP - 1];
      if (IM < 0 || IM >= IP) IM = 0;
      if (MMAT == 46 && MSTJ[26] == 1) {
        IM = 0;
      } else if (MMAT == 46 && MSTJ[26] >= 2 && IM != 0) {
        if (K[1][IM - 1] == 94) {
          IM = K[2][K[2][IM - 1] - 1];
          if (IM < 0 || IM >= IP) IM = 0;
        }
      }
      int KFAM = 0;
      if (IM != 0) KFAM = std::abs(K[1][IM - 1]);
      if (IM != 0 && MMAT == 3) {
        int ISIS = 0;
        for (int IL = std::max(IP - 2, IM + 1); IL < std::min(IP + 2, N) + 1; ++IL) {
          if (K[2][IL - 1] == IM) NM++;
          if (K[2][IL - 1] == IM && IL != IP) ISIS = IL;
        }
        if (NM != 2 || KFAM <= 100 || KFAM % 10 != 1 || (KFAM / 1000) % 10 != 0) NM = 0;
        if (NM == 2) {
          KFAS = std::abs(K[1][ISIS - 1]);
          if ((KFAS <= 100 || KFAS % 10 != 1 || (KFAS / 1000) % 10 != 0) && KFAS != 22) NM = 0;
        }
      } else if (IM != 0 && MMAT == 46) {
        MSGN = (K[1][IM - 1] * K[1][IP - 1] < 0) ? -1 : 1;
        if (KFAM > 100 && (KFAM / 1000) % 10 == 0) MSGN *= std::pow(-1, (KFAM / 100) % 10);
      }
    }

    // Kinematics of one-particle decays.
    if (ND == 1) {
      for (int J = 0; J < 4; ++J) {
        P[J][N] = P[J][IP - 1];
      }

      // goto 660 statement
      N += ND;
      if (MBST == 1) {
        for (int J = 0; J < 3; ++J) {
          BE[J] = P[J][IP - 1] / P[3][IP - 1];
        }
        double GA = P[3][IP - 1] / P[4][IP - 1];
        for (int Ii = NSAV; Ii < N; ++Ii) {
          double BEP = BE[0] * P[0][Ii] + BE[1] * P[1][Ii] + BE[2] * P[2][Ii];
          for (int J = 0; J < 3; ++J) {
            P[J][Ii] += GA * (GA * BEP / (1. + GA) + P[3][Ii]) * BE[J];
          }
          P[3][Ii] = GA * (P[3][Ii] + BEP);
        }
      }

      // Fill in position of decay vertex.
      for (int Ii = NSAV; Ii < N; ++Ii) {
        for (int J = 0; J < 4; ++J) {
          V[J][Ii] = VDCY[J];
        }
        V[4][Ii] = 0.;
      }

      LUDECY_setupPartonShowerEvolution(IP, NSAV, MMAT, ND, MMIX);
      return;
    }

    // Calculate maximum weight ND-particle decay.
    PV[4][ND - 1] = P[4][N + ND - 1];
    double WTMAX = 0.;
    if (ND >= 3) {
      WTMAX = 1. / WTCOR[ND - 3];
      double PMAX = PV[4][0] - PS + P[4][N + ND - 1];
      double PMIN = 0.;
      for (int IL = ND - 1; IL > 0; --IL) {
        PMAX += P[4][N + IL - 1];
        PMIN += P[4][N + IL];
        WTMAX *= PAWT(PMAX, PMIN, P[4][N + IL - 1]);
      }
    }

    // Find virtual gamma mass in Dalitz decay.
    bool goto420 = true;
    bool goto490 = false;
    do {  // 420, 490
      if (goto420 && goto490) throw std::runtime_error("LUDECY (2): error in goto arrangement");

      if (goto420) {
        goto420 = false;

        if (ND == 2) {
          // do nothing
        } else if (MMAT == 2) {
          PMES = 4. * PMAS[0][10] * PMAS[0][10];
          double PMRHO2 = PMAS[0][130] * PMAS[0][130];
          double PGRHO2 = PMAS[1][130] * PMAS[1][130];
          do {
            PMST = PMES * std::pow(P[4][IP - 1] * P[4][IP - 1] / PMES, RLU());
            WT = (1 + 0.5 * PMES / PMST) * std::sqrt(std::max(0., 1. - PMES / PMST)) *
                 std::pow(1. - PMST / (P[4][IP - 1] * P[4][IP - 1]), 3) * (1. + PGRHO2 / PMRHO2) /
                 ((1. - PMST / PMRHO2) * (1. - PMST / PMRHO2) + PGRHO2 / PMRHO2);
          } while (WT < RLU());
          PV[4][1] = std::max(2.00001 * PMAS[0][10], std::sqrt(PMST));

          // M-generator gives weight. If rejected, try again.
        } else {
          do {
            RORD[0] = 1.;
            for (int IL1 = 2; IL1 < ND; ++IL1) {
              double RSAV = RLU();
              int IL2tmp = 0;
              for (int IL2 = IL1 - 1; IL2 > 0; --IL2) {
                IL2tmp = IL2;
                if (RSAV <= RORD[IL2 - 1]) break;
                RORD[IL2] = RORD[IL2 - 1];
              }
              RORD[IL2tmp] = RSAV;
            }
            RORD[ND - 1] = 0.;
            WT = 1.;
            for (int IL = ND - 1; IL > 0; --IL) {
              PV[4][IL - 1] =
                  PV[4][IL] + P[4][N + IL - 1] + (RORD[IL - 1] - RORD[IL]) * (PV[4][0] - PS);
              WT *= PAWT(PV[4][IL - 1], PV[4][IL], P[4][N + IL - 1]);
            }
          } while (WT < RLU() * WTMAX);
        }
      }

      // 490
      goto490 = false;

      // Perform two-particle decays in respective CM frame.
      for (int IL = 1; IL < ND; ++IL) {
        double PA = PAWT(PV[4][IL - 1], PV[4][IL], P[4][N + IL - 1]);
        UE[2] = 2. * RLU() - 1.;
        double PHI = PARU[1] * RLU();
        UE[0] = std::sqrt(1. - UE[2] * UE[2]) * std::cos(PHI);
        UE[1] = std::sqrt(1. - UE[2] * UE[2]) * std::sin(PHI);
        for (int J = 0; J < 3; ++J) {
          P[J][N + IL - 1] = PA * UE[J];
          PV[J][IL] = -PA * UE[J];
        }
        P[3][N + IL - 1] = std::sqrt(PA * PA + P[4][N + IL - 1] * P[4][N + IL - 1]);
        PV[3][IL] = std::sqrt(PA * PA + PV[4][IL] * PV[4][IL]);
      }

      // Lorentz transform decay products to lab frame.
      for (int J = 0; J < 4; ++J) {
        P[J][N + ND - 1] = PV[J][ND - 1];
      }
      for (int IL = ND - 1; IL > 0; --IL) {
        for (int J = 0; J < 3; ++J) {
          BE[J] = PV[J][IL - 1] / PV[3][IL - 1];
        }
        double GA = PV[3][IL - 1] / PV[4][IL - 1];
        for (int Ii = N + IL; Ii < N + ND + 1; ++Ii) {
          double BEP = BE[0] * P[0][Ii - 1] + BE[1] * P[1][Ii - 1] + BE[2] * P[2][Ii - 1];
          for (int J = 0; J < 3; ++J) {
            P[J][Ii - 1] += GA * (GA * BEP / (1. + GA) + P[3][Ii - 1]) * BE[J];
          }
          P[3][Ii - 1] = GA * (P[3][Ii - 1] + BEP);
        }
      }

      // Check that no infinite loop in matrix element weight.
      NTRY++;
      if (NTRY <= 800) {  // skip condition 590

        // Matrix elements for omega and phi decays.
        if (MMAT == 1) {
          WT = std::pow(P[4][N] * P[4][N + 1] * P[4][N + 2], 2) -
               std::pow(P[4][N] * FOUR(N + 2, N + 3), 2) -
               std::pow(P[4][N + 1] * FOUR(N + 1, N + 3), 2) -
               std::pow(P[4][N + 2] * FOUR(N + 1, N + 2), 2) +
               2. * FOUR(N + 1, N + 2) * FOUR(N + 1, N + 3) * FOUR(N + 2, N + 3);
          if (std::max(WT * WTCOR[8] / std::pow(P[4][IP - 1], 6), 0.001) < RLU()) {
            goto420 = true;
            goto490 = false;
            continue;
          }

          // Matrix elements for pi0 or eta Dalitz decay to gamma e+ e-.
        } else if (MMAT == 2) {
          double FOUR12 = FOUR(N + 1, N + 2);
          double FOUR13 = FOUR(N + 1, N + 3);
          WT = (PMST - 0.5 * PMES) * (FOUR12 * FOUR12 + FOUR13 * FOUR13) +
               PMES * (FOUR12 * FOUR13 + FOUR12 * FOUR12 + FOUR13 * FOUR13);
          if (WT < RLU() * 0.25 * PMST * std::pow(P[4][IP - 1] * P[4][IP - 1] - PMST, 2)) {
            goto420 = false;
            goto490 = true;
            continue;
          }

          // Matrix element for S0 -> S1 + V1 -> S1 + S2 + S3 (S scalar,
          // V vector), of form cos^2(theta02) in V1 rest frame, and for
          // S0 -> gamma + V1 -> gamma + S2 + S3, of form sin^2(theta02).
        } else if (MMAT == 3 && NM == 2) {
          double FOUR10 = FOUR(IP, IM);
          double FOUR12 = FOUR(IP, N + 1);
          double FOUR02 = FOUR(IM, N + 1);
          double PMS1 = P[4][IP - 1] * P[4][IP - 1];
          double PMS0 = P[4][IM - 1] * P[4][IM - 1];
          double PMS2 = P[4][N] * P[4][N];
          double HNUM = 0.;
          if (KFAS != 22) HNUM = std::pow(FOUR10 * FOUR12 - PMS1 * FOUR02, 2);
          if (KFAS == 22)
            HNUM = PMS1 * (2. * FOUR10 * FOUR12 * FOUR02 - PMS1 * FOUR02 * FOUR02 -
                           PMS0 * FOUR12 * FOUR12 - PMS2 * FOUR10 * FOUR10 + PMS1 * PMS0 * PMS2);
          HNUM = std::max(1e-6 * PMS1 * PMS1 * PMS0 * PMS2, HNUM);
          double HDEN = (FOUR10 * FOUR10 - PMS1 * PMS0) * (FOUR12 * FOUR12 - PMS1 * PMS2);
          if (HNUM < RLU() * HDEN) {
            goto420 = false;
            goto490 = true;
            continue;
          }

          // Matrix element for "onium" -> g + g + g or gamma + g + g.
        } else if (MMAT == 4) {
          double HX1 = 2. * FOUR(IP, N + 1) / (P[4][IP - 1] * P[4][IP - 1]);
          double HX2 = 2. * FOUR(IP, N + 2) / (P[4][IP - 1] * P[4][IP - 1]);
          double HX3 = 2. * FOUR(IP, N + 3) / (P[4][IP - 1] * P[4][IP - 1]);
          WT = std::pow((1. - HX1) / (HX2 * HX3), 2) + std::pow((1. - HX2) / (HX1 * HX3), 2) +
               std::pow((1. - HX3) / (HX1 * HX2), 2);
          if (WT < 2. * RLU()) {
            goto420 = true;
            goto490 = false;
            continue;
          }
          if (K[1][IP] == 22 &&
              (1. - HX1) * P[4][IP - 1] * P[4][IP - 1] < 4. * PARJ[31] * PARJ[31]) {
            goto420 = true;
            goto490 = false;
            continue;
          }

          // Effective matrix element for nu spectrum in tau -> nu + hadrons.
        } else if (MMAT == 41) {
          double HX1 = 2. * FOUR(IP, N + 1) / (P[4][IP - 1] * P[4][IP - 1]);
          double HXM = std::min(0.75, 2. * (1. - PS / P[4][IP - 1]));
          if (HX1 * (3. - 2. * HX1) < RLU() * HXM * (3. - 2. * HXM)) {
            goto420 = true;
            goto490 = false;
            continue;
          }

          // Matrix elements for weak decays (only semileptonic for c and b)
        } else if ((MMAT == 42 || MMAT == 43 || MMAT == 44 || MMAT == 48) && ND == 3) {
          if (MBST == 0) WT = FOUR(IP, N + 1) * FOUR(N + 2, N + 3);
          if (MBST == 1) WT = P[4][IP - 1] * P[3][N] * FOUR(N + 2, N + 3);
          if (WT < RLU() * P[4][IP - 1] * std::pow(PV[4][0], 3) / WTCOR[9]) {
            goto420 = true;
            goto490 = false;
            continue;
          }
        } else if (MMAT == 42 || MMAT == 43 || MMAT == 44 || MMAT == 48) {
          for (int J = 0; J < 4; ++J) {
            P[J][N + NP] = 0.;
            for (int IS = N + 2; IS < N + NP; ++IS) {
              P[J][N + NP] += P[J][IS];
            }
          }
          if (MBST == 0) WT = FOUR(IP, N + 1) * FOUR(N + 2, N + NP + 1);
          if (MBST == 1) WT = P[4][IP - 1] * P[3][N] * FOUR(N + 2, N + NP + 1);
          if (WT < RLU() * P[4][IP - 1] * std::pow(PV[4][0], 3) / WTCOR[9]) {
            goto420 = true;
            goto490 = false;
            continue;
          }

          // Angular distribution in W decay.
        } else if (MMAT == 46 && MSGN != 0) {
          if (MSGN > 0) WT = FOUR(IM, N + 1) * FOUR(N + 2, IP + 1);
          if (MSGN < 0) WT = FOUR(IM, N + 2) * FOUR(N + 1, IP + 1);
          if (WT < RLU() * std::pow(P[4][IM - 1], 4) / WTCOR[9]) {
            goto420 = false;
            goto490 = true;
            continue;
          }
        }
      }

      // Scale back energy and reattach spectator.
      if (MREM == 1) {
        for (int J = 0; J < 5; ++J) {
          PV[J][0] /= (1. - PQT);
        }
        ND++;
        MREM = 0;
      }

      // Low invariant mass for system with spectator quark gives particle,
      // not two jets. Readjust momenta accordingly.
      if ((MMAT == 31 || MMAT == 45) && ND == 3) {
        MSTJ[92] = 1;
        double PM2 = ULMASS(K[1][N + 1]);
        MSTJ[92] = 1;
        double PM3 = ULMASS(K[1][N + 2]);
        if (P[4][N + 1] * P[4][N + 1] + P[4][N + 2] * P[4][N + 2] + 2. * FOUR(N + 2, N + 3) >=
            (PARJ[31] + PM2 + PM3) * (PARJ[31] + PM2 + PM3)) {
          // goto 660 statement
          N += ND;
          if (MBST == 1) {
            for (int J = 0; J < 3; ++J) {
              BE[J] = P[J][IP - 1] / P[3][IP - 1];
            }
            double GA = P[3][IP - 1] / P[4][IP - 1];
            for (int Ii = NSAV; Ii < N; ++Ii) {
              double BEP = BE[0] * P[0][Ii] + BE[1] * P[1][Ii] + BE[2] * P[2][Ii];
              for (int J = 0; J < 3; ++J) {
                P[J][Ii] += GA * (GA * BEP / (1. + GA) + P[3][Ii]) * BE[J];
              }
              P[3][Ii] = GA * (P[3][Ii] + BEP);
            }
          }

          // Fill in position of decay vertex.
          for (int Ii = NSAV; Ii < N; ++Ii) {
            for (int J = 0; J < 4; ++J) {
              V[J][Ii] = VDCY[J];
            }
            V[4][Ii] = 0.;
          }

          LUDECY_setupPartonShowerEvolution(IP, NSAV, MMAT, ND, MMIX);
          return;
        }
        K[0][N + 1] = 1;
        int KFTEMP = K[1][N + 1];
        std::vector<int> LUKFDI_output = LUKFDI(KFTEMP, K[1][N + 2]);
        K[1][N + 1] = LUKFDI_output[1];
        if (K[1][N + 1] == 0) {
          goto240 = false;
          goto260 = true;
          goto290 = false;
          break;
        }
        P[4][N + 1] = ULMASS(K[1][N + 1]);
        PS = P[4][N] + P[4][N + 1];
        PV[4][1] = P[4][N + 1];
        MMAT = 0;
        ND = 2;

        goto420 = true;
        goto490 = false;
        continue;
      } else if (MMAT == 44) {
        MSTJ[92] = 1;
        double PM3 = ULMASS(K[1][N + 2]);
        MSTJ[92] = 1;
        double PM4 = ULMASS(K[1][N + 3]);
        bool goto630 =
            P[4][N + 2] * P[4][N + 2] + P[4][N + 3] * P[4][N + 3] + 2. * FOUR(N + 3, N + 4) >=
            (PARJ[31] + PM3 + PM4) * (PARJ[31] + PM3 + PM4);
        if (goto630 == false) {  // skip condition 630
          K[0][N + 2] = 1;
          int KFTEMP = K[1][N + 2];
          std::vector<int> LUKFDI_output = LUKFDI(KFTEMP, K[1][N + 3]);
          LUKFDI_output[1] = K[1][N + 2];
          if (K[1][N + 2] == 0) {
            goto240 = false;
            goto260 = true;
            goto290 = false;
            break;
          }
          P[4][N + 2] = ULMASS(K[1][N + 2]);
          for (int J = 0; J < 3; ++J) {
            P[J][N + 2] += P[J][N + 3];
          }
          P[3][N + 2] = std::sqrt(P[0][N + 2] * P[0][N + 2] + P[1][N + 2] * P[1][N + 2] +
                                  P[2][N + 2] * P[2][N + 2] + P[4][N + 2] * P[4][N + 2]);
          double HA = P[3][N] * P[3][N] - P[3][N + 1] * P[3][N + 1];
          double HB = HA - (P[4][N] * P[4][N] - P[4][N + 1] * P[4][N + 1]);
          double HC = (P[0][N] - P[0][N + 1]) * (P[0][N] - P[0][N + 1]) +
                      (P[1][N] - P[1][N + 1]) * (P[1][N] - P[1][N + 1]) +
                      (P[2][N] - P[2][N + 1]) * (P[2][N] - P[2][N + 1]);
          double HD = (PV[3][0] - P[3][N + 2]) * (PV[3][0] - P[3][N + 2]);
          double HE = HA * HA - 2. * HD * (P[3][N] * P[3][N] + P[3][N + 1] * P[3][N + 1]) + HD * HD;
          double HF = HD * HC - HB * HB;
          double HG = HD * HC - HA * HB;
          double HH = (std::sqrt(HG * HG + HE * HF) - HG) / (2. * HF);
          for (int J = 0; J < 3; ++J) {
            double PCOR = HH * (P[J][N] - P[J][N + 1]);
            P[J][N] += PCOR;
            P[J][N + 1] -= PCOR;
          }
          P[3][N] = std::sqrt(P[0][N] * P[0][N] + P[1][N] * P[1][N] + P[2][N] * P[2][N] +
                              P[4][N] * P[4][N]);
          P[3][N + 1] = std::sqrt(P[0][N + 1] * P[0][N + 1] + P[1][N + 1] * P[1][N + 1] +
                                  P[2][N + 1] * P[2][N + 1] + P[4][N + 1] * P[4][N + 1]);
          ND--;
        }
      }

      // Check invariant mass of W jets. May give one particle or start over.
      double PMR = 0.;
      do {  // skip conditions 640
        if ((MMAT == 42 || MMAT == 43 || MMAT == 44 || MMAT == 48) && std::abs(K[1][N]) < 10) {
          PMR = std::sqrt(std::max(
              0., P[4][N] * P[4][N] + P[4][N + 1] * P[4][N + 1] + 2. * FOUR(N + 1, N + 2)));
          MSTJ[92] = 1;
          double PM1 = ULMASS(K[1][N]);
          MSTJ[92] = 1;
          double PM2 = ULMASS(K[1][N + 1]);
          if (PMR > PARJ[31] + PM1 + PM2) break;
          int KFLDUM = static_cast<int>(1.5 + RLU());
          int sgn1 = (K[1][N] < 0) ? -1 : 1;
          std::vector<int> LUKFDI_output1 = LUKFDI(K[1][N], -KFLDUM * sgn1);
          int KF1 = LUKFDI_output1[1];
          int sgn2 = (K[1][N + 1] < 0) ? -1 : 1;
          std::vector<int> LUKFDI_output2 = LUKFDI(K[1][N + 1], -KFLDUM * sgn2);
          int KF2 = LUKFDI_output2[1];
          if (KF1 == 0 || KF2 == 0) {
            goto240 = false;
            goto260 = true;
            goto290 = false;
            break;
          }
          double PSM = ULMASS(KF1) + ULMASS(KF2);
          if ((MMAT == 42 || MMAT == 48) && PMR > PARJ[63] + PSM) break;
          if (MMAT >= 43 && PMR > 0.2 * PARJ[31] + PSM) break;
          if (MMAT == 48) {
            goto420 = true;
            goto490 = false;
            break;
          }
          if (ND == 4 || KFA == 15) {
            goto240 = false;
            goto260 = true;
            goto290 = false;
            break;
          }
          K[0][N] = 1;
          int KFTEMP = K[1][N];
          std::vector<int> LUKFDI_output = LUKFDI(KFTEMP, K[1][N + 1]);
          K[1][N] = LUKFDI_output[1];
          if (K[1][N] == 0) {
            goto240 = false;
            goto260 = true;
            goto290 = false;
            break;
          }
          P[4][N] = ULMASS(K[1][N]);
          K[1][N + 1] = K[1][N + 2];
          P[4][N + 1] = P[4][N + 2];
          double PS = P[4][N] + P[4][N + 1];
          if (PS + PARJ[63] > PV[4][0]) {
            goto240 = false;
            goto260 = true;
            goto290 = false;
            break;
          }
          PV[4][1] = P[4][N + 2];
          MMAT = 0;
          ND = 2;

          goto420 = false;
          goto490 = true;
          break;
        }
      } while (false);  // end skip condtitions 630
      if (goto240 || goto260 || goto290) break;
      if (goto420 || goto490) continue;

      // 640
      // Phase space decay of partons from W decay.
      if ((MMAT == 42 || MMAT == 48) && std::abs(K[1][N]) < 10) {
        KFLO[0] = K[1][N];
        KFLO[1] = K[1][N + 1];
        K[0][N] = K[0][N + 2];
        K[1][N] = K[1][N + 2];
        for (int J = 0; J < 5; ++J) {
          PV[J][0] = P[J][N] + P[J][N + 1];
          P[J][N] = P[J][N + 2];
        }
        PV[4][0] = PMR;
        N++;
        NP = 0;
        NQ = 2;
        PS = 0.;
        MSTJ[92] = 2;
        PSQ = ULMASS(KFLO[0]);
        MSTJ[92] = 2;
        PSQ += ULMASS(KFLO[1]);
        MMAT = 11;

        goto240 = false;
        goto260 = false;
        goto290 = true;
        break;
      }
    } while (goto420 || goto490);
  } while (goto240 || goto260 || goto290);

  // 660
  // Boost back for rapidly moving particle.
  N += ND;
  if (MBST == 1) {
    for (int J = 0; J < 3; ++J) {
      BE[J] = P[J][IP - 1] / P[3][IP - 1];
    }
    double GA = P[3][IP - 1] / P[4][IP - 1];
    for (int Ii = NSAV; Ii < N; ++Ii) {
      double BEP = BE[0] * P[0][Ii] + BE[1] * P[1][Ii] + BE[2] * P[2][Ii];
      for (int J = 0; J < 3; ++J) {
        P[J][Ii] += GA * (GA * BEP / (1. + GA) + P[3][Ii]) * BE[J];
      }
      P[3][Ii] = GA * (P[3][Ii] + BEP);
    }
  }

  // Fill in position of decay vertex.
  for (int Ii = NSAV; Ii < N; ++Ii) {
    for (int J = 0; J < 4; ++J) {
      V[J][Ii] = VDCY[J];
    }
    V[4][Ii] = 0.;
  }

  LUDECY_setupPartonShowerEvolution(IP, NSAV, MMAT, ND, MMIX);
  return;
}

void sophia_interface::LUDECY_setupPartonShowerEvolution(int IP, int NSAV, int MMAT, int ND,
                                                         int MMIX) {
  // Set up for parton shower evolution from jets.
  // This function is only called by LUDECY. After this function was called, LUDECY returns.
  if (MSTJ[22] >= 1 && MMAT == 4 && K[1][NSAV] == 21) {
    K[0][NSAV] = 3;
    K[0][NSAV + 1] = 3;
    K[0][NSAV + 2] = 3;
    K[3][NSAV] = MSTU[4] * (NSAV + 2);
    K[4][NSAV] = MSTU[4] * (NSAV + 3);
    K[3][NSAV + 1] = MSTU[4] * (NSAV + 3);
    K[4][NSAV + 1] = MSTU[4] * (NSAV + 1);
    K[3][NSAV + 2] = MSTU[4] * (NSAV + 1);
    K[4][NSAV + 2] = MSTU[4] * (NSAV + 2);
    MSTJ[91] = -(NSAV + 1);
  } else if (MSTJ[22] >= 1 && MMAT == 4) {
    K[0][NSAV + 1] = 3;
    K[0][NSAV + 2] = 3;
    K[3][NSAV + 1] = MSTU[4] * (NSAV + 3);
    K[4][NSAV + 1] = MSTU[4] * (NSAV + 3);
    K[3][NSAV + 2] = MSTU[4] * (NSAV + 2);
    K[4][NSAV + 2] = MSTU[4] * (NSAV + 2);
    MSTJ[91] = NSAV + 2;
  } else if (MSTJ[22] >= 1 && (MMAT == 32 || MMAT == 44 || MMAT == 46) &&
             std::abs(K[1][NSAV]) <= 10 && std::abs(K[1][NSAV + 1]) <= 10) {
    K[0][NSAV] = 3;
    K[0][NSAV + 1] = 3;
    K[3][NSAV] = MSTU[4] * (NSAV + 2);
    K[4][NSAV] = MSTU[4] * (NSAV + 2);
    K[3][NSAV + 1] = MSTU[4] * (NSAV + 1);
    K[4][NSAV + 1] = MSTU[4] * (NSAV + 1);
    MSTJ[91] = NSAV + 1;
  } else if (MSTJ[22] >= 1 && (MMAT == 32 || MMAT == 44 || MMAT == 46) &&
             std::abs(K[1][NSAV]) <= 20 && std::abs(K[1][NSAV + 1]) <= 20) {
    MSTJ[91] = NSAV + 1;
  } else if (MSTJ[22] >= 1 && MMAT == 33 && std::abs(K[1][NSAV + 1]) == 21) {
    K[0][NSAV] = 3;
    K[0][NSAV + 1] = 3;
    K[0][NSAV + 2] = 3;
    int KCP = LUCOMP(K[1][NSAV]);
    int sgn = (K[1][NSAV] < 0) ? -1 : 1;
    int KQP = KCHG[1][KCP - 1] * sgn;
    int JCON = 4;
    if (KQP < 0) JCON = 5;
    K[JCON - 1][NSAV] = MSTU[4] * (NSAV + 2);
    K[8 - JCON][NSAV + 1] = MSTU[4] * (NSAV + 1);
    K[JCON - 1][NSAV + 1] = MSTU[4] * (NSAV + 3);
    K[8 - JCON][NSAV + 2] = MSTU[4] * (NSAV + 2);
    MSTJ[91] = NSAV + 1;
  } else if (MSTJ[22] >= 1 && MMAT == 33) {
    K[0][NSAV] = 3;
    K[0][NSAV + 2] = 3;
    K[3][NSAV] = MSTU[4] * (NSAV + 3);
    K[4][NSAV] = MSTU[4] * (NSAV + 3);
    K[3][NSAV + 2] = MSTU[4] * (NSAV + 1);
    K[4][NSAV + 2] = MSTU[4] * (NSAV + 1);
    MSTJ[91] = NSAV + 1;

    // Set up for parton shower evolution in t -> W + b.
  } else if (MSTJ[26] >= 1 && MMAT == 45 && ND == 3) {
    K[0][NSAV + 1] = 3;
    K[0][NSAV + 2] = 3;
    K[3][NSAV + 1] = MSTU[4] * (NSAV + 3);
    K[4][NSAV + 1] = MSTU[4] * (NSAV + 3);
    K[3][NSAV + 2] = MSTU[4] * (NSAV + 2);
    K[4][NSAV + 2] = MSTU[4] * (NSAV + 2);
    MSTJ[91] = NSAV + 1;
  }

  // Mark decayed particle; special option for B-B~ mixing.
  if (K[0][IP - 1] == 5) K[0][IP - 1] = 15;
  if (K[0][IP - 1] <= 10) K[0][IP - 1] = 11;
  if (MMIX == 1 && MSTJ[25] == 2 && K[0][IP - 1] == 11) K[0][IP - 1] = 12;
  K[3][IP - 1] = NSAV + 1;
  K[4][IP - 1] = N;

  return;
}

void sophia_interface::LUSTRF(int IP) {
  // Purpose: to handle the fragmentation of an arbitrary colour singlet
  // jet system according to the Lund string fragmentation model.
  double DPS[5] = {0.};
  int KFL[3] = {0};
  double PMQ[3] = {0.};
  double PX[3] = {0.};
  double PY[3] = {0.};
  double GAM[3] = {0.};
  int IE[2] = {0};
  double PR[2] = {0.};
  int IN[9] = {0};
  double DHM[4] = {0.};
  double DHG[4] = {0.};
  double DP[5][5] = {0.};
  int IRANK[2] = {0};
  int MJU[4] = {0};
  int IJU[3] = {0};
  double PJU[5][5] = {0.};
  double TJU[5] = {0.};
  int KFJH[2] = {0};
  int NJS[2] = {0};
  int KFJS[2] = {0};
  double PJS[5][4] = {0.};
  int MSTU9T[8] = {0};
  double PARU9T[8] = {0.};

  // Function: four-product of two vectors.
  std::function<double(int, int)> FOUR = [&](int I, int J) {
    return P[3][I - 1] * P[3][J - 1] - P[0][I - 1] * P[0][J - 1] - P[1][I - 1] * P[1][J - 1] -
           P[2][I - 1] * P[2][J - 1];
  };
  std::function<double(int, int)> DFOUR = [&](int I, int J) {
    return DP[3][I - 1] * DP[3][J - 1] - DP[0][I - 1] * DP[0][J - 1] - DP[1][I - 1] * DP[1][J - 1] -
           DP[2][I - 1] * DP[2][J - 1];
  };

  // Premature initializations for goto 820 pointer
  bool goto900 = false;
  bool goto500 = false;
  double PARJST = 0.;
  int MSTU91 = MSTU[89];
  double Z = 0.;
  int KFL1A = 0;
  int KFL2A = 0;
  double WMIN = 0.;
  double WREM2 = 0.;
  bool repeat780 = false;
  int JR = 0;
  int JS = 0;
  int JT = 0;
  int NS = 0;
  int NB = 0;
  int ISAV = 0;
  std::vector<int> LUKFDI_output;
  std::vector<double> LUPTDI_output;
  int MBST = 0;
  double HHBZ = 0.;

  // Reset counters. Identify parton system.
  MSTJ[90] = 0;
  int NSAV = N;
  int MSTU90 = MSTU[89];
  int NP = 0;
  int KQSUM = 0;
  MJU[0] = 0;
  MJU[1] = 0;
  int I = IP - 1;

  bool repeat110 = false;
  int KQ = 0;
  do {
    repeat110 = false;
    I++;
    if (I > std::min(N, MSTU[3] - MSTU[31])) {
      LUERRM(12, "LUSTRF (1): failed to reconstruct jet system");
      if (MSTU[20] >= 1) return;
    }
    if (K[0][I - 1] != 1 && K[0][I - 1] != 2 && K[0][I - 1] != 41) {
      repeat110 = true;
      continue;
    }
    int KC = LUCOMP(K[1][I - 1]);
    if (KC == 0) {
      repeat110 = true;
      continue;
    }
    int sgn = (K[1][I - 1] < 0) ? -1 : 1;
    KQ = KCHG[1][KC - 1] * sgn;
    if (KQ == 0) {
      repeat110 = true;
      continue;
    }
    if (N + 5 * NP + 11 > MSTU[3] - MSTU[31] - 5) {
      LUERRM(11, "LUSTRF (2): no more memory left in LUJETS");
      if (MSTU[20] >= 1) return;
    }

    // Take copy of partons to be considered. Check flavour sum.
    NP++;
    for (int J = 0; J < 5; ++J) {
      K[J][N + NP - 1] = K[J][I - 1];
      P[J][N + NP - 1] = P[J][I - 1];
      if (J + 1 != 4) DPS[J] += P[J][I - 1];
    }
    DPS[3] += std::sqrt(P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1] +
                        P[2][I - 1] * P[2][I - 1] + P[4][I - 1] * P[4][I - 1]);
    K[2][N + NP - 1] = I;
    if (KQ != 2) KQSUM += KQ;
    if (K[0][I - 1] == 41) {
      KQSUM += 2 * KQ;
      if (KQSUM == KQ) MJU[0] = N + NP;
      if (KQSUM != KQ) MJU[1] = N + NP;
    }
    if (K[0][I - 1] == 2 || K[0][I - 1] == 41) {
      repeat110 = true;
      continue;
    }
  } while (repeat110);
  if (KQSUM != 0) {
    LUERRM(12, "LUSTRF (3): unphysical flavour combination");
    if (MSTU[20] >= 1) return;
  }

  // Boost copied system to CM frame (for better numerical precision).
  if (std::abs(DPS[2]) < 0.99 * DPS[3]) {
    MBST = 0;
    MSTU[32] = 1;
    LUDBRB(N + 1, N + NP, 0., 0., -DPS[0] / DPS[3], -DPS[1] / DPS[3], -DPS[2] / DPS[3]);
  } else {
    MBST = 1;
    HHBZ = std::sqrt(std::max(1e-6, DPS[3] + DPS[2]) / std::max(1e-6, DPS[3] - DPS[2]));
    for (int Ii = N; Ii < N + NP; ++Ii) {
      double HHPMT = P[0][Ii] * P[0][Ii] + P[1][Ii] * P[1][Ii] + P[4][Ii] * P[4][Ii];
      if (P[2][Ii] > 0.) {
        double HHPEZ = (P[3][Ii] + P[2][Ii]) / HHBZ;
        P[2][Ii] = 0.5 * (HHPEZ - HHPMT / HHPEZ);
        P[3][Ii] = 0.5 * (HHPEZ + HHPMT / HHPEZ);
      } else {
        double HHPEZ = (P[3][Ii] - P[2][Ii]) * HHBZ;
        P[2][Ii] = -0.5 * (HHPEZ - HHPMT / HHPEZ);
        P[3][Ii] = 0.5 * (HHPEZ + HHPMT / HHPEZ);
      }
    }
  }

  // Search for very nearby partons that may be recombined.
  int NTRYR = 0;
  double PARU12 = PARU[11];
  double PARU13 = PARU[12];
  MJU[2] = MJU[0];
  MJU[3] = MJU[1];
  int NR = NP;

label_140:  // this is kind-of a do-while loop

  if (NR >= 3) {
    double PDRMIN = 2. * PARU12;
    int IR = 0;
    for (int Ii = N + 1; Ii < N + NR + 1; ++Ii) {
      if (Ii == N + NR && std::abs(K[1][N]) != 21) continue;
      int I1 = Ii + 1;
      if (Ii == N + NR) I1 = N + 1;
      if (K[0][Ii - 1] == 41 || K[0][I1 - 1] == 41) continue;
      if (MJU[0] != 0 && I1 < MJU[0] && std::abs(K[1][I1 - 1]) != 21) continue;
      if (MJU[1] != 0 && Ii > MJU[1] && std::abs(K[1][Ii - 1]) != 21) continue;
      double PAP = std::sqrt((P[0][Ii - 1] * P[0][Ii - 1] + P[1][Ii - 1] * P[1][Ii - 1] +
                              P[2][Ii - 1] * P[2][Ii - 1]) *
                             (P[0][I1 - 1] * P[0][I1 - 1] + P[1][I1 - 1] * P[1][I1 - 1] +
                              P[2][I1 - 1] * P[2][I1 - 1]));
      double PVP =
          P[0][Ii - 1] * P[0][I1 - 1] + P[1][Ii - 1] * P[1][I1 - 1] + P[2][Ii - 1] * P[2][I1 - 1];
      double PDR =
          4. * (PAP - PVP) * (PAP - PVP) / std::max(1e-6, PARU13 * PARU13 * PAP + 2. * (PAP - PVP));
      if (PDR < PDRMIN) {
        IR = Ii;
        PDRMIN = PDR;
      }
    }

    // Recombine very nearby partons to avoid machine precision problems.
    if (PDRMIN < PARU12 && IR == N + NR) {
      for (int J = 0; J < 4; ++J) {
        P[J][N] += P[J][N + NR - 1];
      }
      P[4][N] = std::sqrt(std::max(
          0., P[3][N] * P[3][N] - P[0][N] * P[0][N] - P[1][N] * P[1][N] - P[2][N] * P[2][N]));
      NR--;
      goto label_140;
    } else if (PDRMIN < PARU12) {
      for (int J = 0; J < 4; ++J) {
        P[J][IR - 1] += P[J][IR];
      }
      P[4][IR - 1] =
          std::sqrt(std::max(0., P[3][IR - 1] * P[3][IR - 1] - P[0][IR - 1] * P[0][IR - 1] -
                                     P[1][IR - 1] * P[1][IR - 1] - P[2][IR - 1] * P[2][IR - 1]));
      for (int Ii = IR; Ii < N + NR - 1; ++Ii) {
        K[1][Ii] = K[1][Ii + 1];
        for (int J = 0; J < 5; ++J) {
          P[J][Ii] = P[J][Ii + 1];
        }
      }
      if (IR == N + NR - 1) K[1][IR - 1] = K[1][N + NR - 1];
      NR--;
      if (MJU[0] > IR) MJU[0]--;
      if (MJU[1] > IR) MJU[1]--;
      goto label_140;
    }
  }
  NTRYR++;

  // Reset particle counter. Skip ahead if no junctions are present;
  // this is usually the case!
  const int NRS = std::max(5 * NR + 11, NP);
  int NTRY = 0;

label_200:

  NTRY++;
  if (NTRY > 100 && NTRYR <= 4) {
    PARU12 *= 4.;
    PARU13 *= 2.;
    goto label_140;
  } else if (NTRY > 100) {
    LUERRM(14, "LUSTRF (4): caught in infinite loop");
    if (MSTU[20] >= 1) return;
  }

  I = N + NRS;
  MSTU[89] = MSTU90;
  if (MJU[0] != 0 && MJU[1] != 0) {      // skip 580
    for (int JTi = 1; JTi < 3; ++JTi) {  // 570
      NJS[JTi - 1] = 0;
      if (MJU[JTi - 1] == 0) continue;
      int JS = 3 - 2 * JTi;

      // Find and sum up momentum on three sides of junction. Check flavours.
      for (int IU = 0; IU < 3; ++IU) {
        IJU[IU] = 0;
        for (int J = 0; J < 5; ++J) {
          PJU[J][IU] = 0.;
        }
      }
      int IU = 0;
      for (int I1 = N + 1 + (JTi - 1) * (NR - 1); I1 < N + NR + (JTi - 1) * (1 - NR) + 1;
           I1 += JS) {
        if (K[1][I1 - 1] != 21 && IU <= 2) {
          IU++;
          IJU[IU - 1] = I1;
        }
        for (int J = 0; J < 4; ++J) {
          PJU[J][IU - 1] += P[J][I1 - 1];
        }
      }
      for (int IUi = 0; IUi < 3; ++IUi) {
        PJU[4][IUi] = std::sqrt(PJU[0][IUi] * PJU[0][IUi] + PJU[1][IUi] * PJU[1][IUi] +
                                PJU[2][IUi] * PJU[2][IUi]);
      }
      if (K[1][IJU[2] - 1] / 100 != 10 * K[1][IJU[0] - 1] + K[1][IJU[1] - 1] &&
          K[1][IJU[2] - 1] / 100 != 10 * K[1][IJU[1] - 1] + K[1][IJU[0] - 1]) {
        LUERRM(12, "LUSTRF: (5) unphysical flavour combination");
        if (MSTU[20] >= 1) return;
      }

      // Calculate (approximate) boost to rest frame of junction.
      double T12 = (PJU[0][0] * PJU[0][1] + PJU[1][0] * PJU[1][1] + PJU[2][0] * PJU[2][1]) /
                   (PJU[4][0] * PJU[4][1]);
      double T13 = (PJU[0][0] * PJU[0][2] + PJU[1][0] * PJU[1][2] + PJU[2][0] * PJU[2][2]) /
                   (PJU[4][0] * PJU[4][2]);
      double T23 = (PJU[0][1] * PJU[0][2] + PJU[1][1] * PJU[1][2] + PJU[2][1] * PJU[2][2]) /
                   (PJU[4][1] * PJU[4][2]);
      double T11 = std::sqrt((2. / 3.) * (1. - T12) * (1. - T13) / (1. - T23));
      double T22 = std::sqrt((2. / 3.) * (1. - T12) * (1. - T23) / (1. - T13));
      double TSQ = std::sqrt((2. * T11 * T22 + T12 - 1.) * (1. + T12));
      double T1F = (TSQ - T22 * (1. + T12)) / (1. - T12 * T12);
      double T2F = (TSQ - T11 * (1. + T12)) / (1. - T12 * T12);
      for (int J = 0; J < 3; ++J) {
        TJU[J] = -(T1F * PJU[J][0] / PJU[4][0] + T2F * PJU[J][1] / PJU[4][1]);
      }
      TJU[3] = std::sqrt(1. + TJU[0] * TJU[0] + TJU[1] * TJU[1] + TJU[2] * TJU[2]);
      for (int IUi = 0; IUi < 3; ++IUi) {
        PJU[4][IUi] = TJU[3] * PJU[3][IUi] - TJU[0] * PJU[0][IUi] - TJU[1] * PJU[1][IUi] -
                      TJU[2] * PJU[2][IUi];
      }

      // Put junction at rest if motion could give inconsistencies.
      if (PJU[4][0] + PJU[4][1] > PJU[3][0] + PJU[3][1]) {
        for (int J = 0; J < 3; ++J) {
          TJU[J] = 0.;
        }
        TJU[3] = 1.;
        PJU[4][0] = PJU[3][0];
        PJU[4][1] = PJU[3][1];
        PJU[4][2] = PJU[3][2];
      }

      // Start preparing for fragmentation of two strings from junction.
      int ISTA = I;
      for (int IUi = 1; IUi < 3; ++IUi) {  // 550
        int NS = IJU[IUi] - IJU[IUi - 1];

        // Junction strings: find longitudinal string directions.
        for (int IS = 1; IS < NS + 1; ++IS) {
          int IS1 = IJU[IUi - 1] + IS - 1;
          int IS2 = IJU[IUi - 1] + IS;
          for (int J = 0; J < 5; ++J) {
            DP[J][0] = 0.5 * P[J][IS1 - 1];
            if (IS == 1) DP[J][0] = P[J][IS1 - 1];
            DP[J][1] = 0.5 * P[J][IS2 - 1];
            if (IS == NS) DP[J][1] = -PJU[J][IUi - 1];
          }
          if (IS == NS)
            DP[3][1] =
                std::sqrt(PJU[0][IUi - 1] * PJU[0][IUi - 1] + PJU[1][IUi - 1] * PJU[1][IUi - 1] +
                          PJU[2][IUi - 1] * PJU[2][IUi - 1]);
          if (IS == NS) DP[4][1] = 0.;
          DP[4][2] = DFOUR(1, 1);
          DP[4][3] = DFOUR(2, 2);
          double DHKC = DFOUR(1, 2);
          if (DP[4][2] + 2. * DHKC + DP[4][3] <= 0.) {
            DP[3][0] = std::sqrt(DP[0][0] * DP[0][0] + DP[1][0] * DP[1][0] + DP[2][0] * DP[2][0]);
            DP[3][1] = std::sqrt(DP[0][1] * DP[0][1] + DP[1][1] * DP[1][1] + DP[2][1] * DP[2][1]);
            DP[4][2] = 0.;
            DP[4][3] = 0.;
            DHKC = DFOUR(1, 2);
          }
          double DHKS = std::sqrt(DHKC * DHKC - DP[4][2] * DP[4][3]);
          double DHK1 = 0.5 * ((DP[4][3] + DHKC) / DHKS - 1.);
          double DHK2 = 0.5 * ((DP[4][2] + DHKC) / DHKS - 1.);
          int IN1 = N + NR + 4 * IS - 3;
          P[4][IN1 - 1] = std::sqrt(DP[4][2] + 2. * DHKC + DP[4][3]);
          for (int J = 0; J < 4; ++J) {
            P[J][IN1 - 1] = (1. + DHK1) * DP[J][0] - DHK2 * DP[J][1];
            P[J][IN1] = (1. + DHK2) * DP[J][1] - DHK1 * DP[J][0];
          }
        }

        // Junction strings: initialize flavour, momentum and starting pos.
        ISAV = I;
        MSTU91 = MSTU[89];

      label_320:  // this is kind-of a do-while loop

        NTRY++;
        if (NTRY > 100 && NTRYR <= 4) {
          PARU12 *= 4.;
          PARU13 *= 2.;
          goto label_140;
        } else if (NTRY > 100) {
          LUERRM(14, "LUSTRF (5): caught in infinite loop");
          if (MSTU[20] >= 1) return;
        }
        I = ISAV;
        MSTU[89] = MSTU91;
        int IRANKJ = 0;
        IE[0] = K[2][N + 1 + (JTi / 2) * (NP - 1) - 1];
        IN[3] = N + NR + 1;
        IN[4] = IN[3] + 1;
        IN[5] = N + NR + 4 * NS + 1;

        for (int JQ = 1; JQ < 3; ++JQ) {
          for (int IN1 = N + NR + 2 + JQ; IN1 < N + NR + 4 * NS - 2 + JQ + 1; IN1 += 4) {
            P[0][IN1 - 1] = 2 - JQ;
            P[1][IN1 - 1] = JQ - 1;
            P[2][IN1 - 1] = 1.;
          }
        }
        KFL[0] = K[1][IJU[IUi - 1] - 1];
        PX[0] = 0.;
        PY[0] = 0.;
        GAM[0] = 0.;
        for (int J = 0; J < 5; ++J) {
          PJU[J][IUi + 2] = 0.;
        }

        // Junction strings: find initial transverse directions.
        for (int J = 0; J < 4; ++J) {
          DP[J][0] = P[J][IN[3] - 1];
          DP[J][1] = P[J][IN[3]];
          DP[J][2] = 0.;
          DP[J][3] = 0.;
        }
        DP[3][0] = std::sqrt(DP[0][0] * DP[0][0] + DP[1][0] * DP[1][0] + DP[2][0] * DP[2][0]);
        DP[3][1] = std::sqrt(DP[0][1] * DP[0][1] + DP[1][1] * DP[1][1] + DP[2][1] * DP[2][1]);
        DP[0][4] = DP[0][0] / DP[3][0] - DP[0][1] / DP[3][1];
        DP[1][4] = DP[1][0] / DP[3][0] - DP[1][1] / DP[3][1];
        DP[2][4] = DP[2][0] / DP[3][0] - DP[2][1] / DP[3][1];
        if (DP[0][4] * DP[0][4] <= DP[1][4] * DP[1][4] + DP[2][4] * DP[2][4]) DP[0][2] = 1.;
        if (DP[0][4] * DP[0][4] > DP[1][4] * DP[1][4] + DP[2][4] * DP[2][4]) DP[2][2] = 1.;
        if (DP[1][4] * DP[1][4] <= DP[0][4] * DP[0][4] + DP[2][4] * DP[2][4]) DP[1][3] = 1.;
        if (DP[1][4] * DP[1][4] > DP[0][4] * DP[0][4] + DP[2][4] * DP[2][4]) DP[2][3] = 1.;
        double DHC12 = DFOUR(1, 2);
        double DHCX1 = DFOUR(3, 1) / DHC12;
        double DHCX2 = DFOUR(3, 2) / DHC12;
        double DHCXX = 1. / std::sqrt(1. + 2. * DHCX1 * DHCX2 * DHC12);
        double DHCY1 = DFOUR(4, 1) / DHC12;
        double DHCY2 = DFOUR(4, 2) / DHC12;
        double DHCYX = DHCXX * (DHCX1 * DHCY2 + DHCX2 * DHCY1) * DHC12;
        double DHCYY = 1. / std::sqrt(1. + 2. * DHCY1 * DHCY2 * DHC12 - DHCYX * DHCYX);
        for (int J = 0; J < 4; ++J) {
          DP[J][2] = DHCXX * (DP[J][2] - DHCX2 * DP[J][0] - DHCX1 * DP[J][1]);
          P[J][IN[5] - 1] = DP[J][2];
          P[J][IN[5]] = DHCYY * (DP[J][3] - DHCY2 * DP[J][0] - DHCY1 * DP[J][1] - DHCYX * DP[J][2]);
        }

        // Junction strings: produce new particle, origin.
        bool repeat380 = false;
        do {  // 380
          repeat380 = false;
          I++;
          if (2 * I - NSAV >= MSTU[3] - MSTU[31] - 5) {
            LUERRM(11, "LUSTRF (6): no more memory left in LUJETS");
            if (MSTU[20] >= 1) return;
          }
          IRANKJ++;
          K[0][I - 1] = 1;
          K[2][I - 1] = IE[0];
          K[3][I - 1] = 0;
          K[4][I - 1] = 0;

          // Junction strings: generate flavour, hadron, pT, z and Gamma.
          bool repeat390 = false;
          do {  // 390
            repeat390 = false;
            LUKFDI_output = LUKFDI(KFL[0], 0);
            KFL[2] = LUKFDI_output[0];
            K[1][I - 1] = LUKFDI_output[1];
            if (K[1][I - 1] == 0) goto label_320;
            if (MSTJ[11] >= 3 && IRANKJ == 1 && std::abs(KFL[0]) <= 10 && std::abs(KFL[2]) > 10) {
              if (RLU() > PARJ[18]) {
                repeat390 = true;
                continue;
              }
            }
          } while (repeat390);
          P[4][I - 1] = ULMASS(K[1][I - 1]);
          LUPTDI_output = LUPTDI(KFL[0]);
          PX[2] = LUPTDI_output[0];
          PY[2] = LUPTDI_output[1];
          PR[0] = P[4][I - 1] * P[4][I - 1] + (PX[0] + PX[2]) * (PX[0] + PX[2]) +
                  (PY[0] + PY[2]) * (PY[0] + PY[2]);
          Z = LUZDIS(KFL[0], KFL[2], PR[0]);
          if (std::abs(KFL[0]) >= 4 && std::abs(KFL[0]) <= 8 && MSTU[89] < 8) {
            MSTU[89]++;
            MSTU[89 + MSTU[89]] = I;
            PARU[89 + MSTU[89]] = Z;
          }
          GAM[2] = (1. - Z) * (GAM[0] + PR[0] / Z);
          for (int J = 0; J < 3; ++J) {
            IN[J] = IN[3 + J];
          }

          goto500 = false;
          // Junction strings: stepping within or from "low" string region easy.
          if (IN[0] + 1 == IN[1] &&
              Z * P[2][IN[0] + 1] * P[2][IN[1] + 1] * P[4][IN[0] - 1] * P[4][IN[0] - 1] >= PR[0]) {
            P[3][IN[0] + 1] = Z * P[2][IN[0] + 1];
            P[3][IN[1] + 1] = PR[0] / (P[3][IN[0] + 1] * P[4][IN[0] - 1] * P[4][IN[0] - 1]);
            for (int J = 0; J < 4; ++J) {
              P[J][I - 1] = (PX[0] + PX[2]) * P[J][IN[2] - 1] + (PY[0] + PY[2]) * P[J][IN[2]];
            }
            goto500 = true;  // skip to 500
          } else if (IN[0] + 1 == IN[1] && goto500 == false) {
            P[3][IN[1] + 1] = P[2][IN[1] + 1];
            P[0][IN[1] + 1] = 1.;
            IN[1] += 4;
            if (IN[1] > N + NR + 4 * NS) goto label_320;
            if (FOUR(IN[0], IN[1]) <= 1e-2) {
              P[3][IN[0] + 1] = P[2][IN[0] + 1];
              P[0][IN[0] + 1] = 0.;
              IN[0] += 4;
            }
          }
          if (goto500 == false) {  // skip to 500
            // Junction strings: find new transverse directions.
            bool repeat420 = false;
            do {  // 420
              repeat420 = false;
              if (IN[0] > N + NR + 4 * NS || IN[1] > N + NR + 4 * NS || IN[0] > IN[1])
                goto label_320;
              if (IN[0] != IN[3] || IN[1] != IN[4]) {
                for (int J = 0; J < 4; ++J) {
                  DP[J][0] = P[J][IN[0] - 1];
                  DP[J][1] = P[J][IN[1] - 1];
                  DP[J][2] = 0.;
                  DP[J][3] = 0.;
                }
                DP[3][0] =
                    std::sqrt(DP[0][0] * DP[0][0] + DP[1][0] * DP[1][0] + DP[2][0] * DP[2][0]);
                DP[3][1] =
                    std::sqrt(DP[0][1] * DP[0][1] + DP[1][1] * DP[1][1] + DP[2][1] * DP[2][1]);
                double DHC12 = DFOUR(1, 2);
                if (DHC12 <= 1e-2) {
                  P[3][IN[0] + 1] = P[2][IN[0] + 1];
                  P[0][IN[0] + 1] = 0.;
                  IN[0] += 4;
                  repeat420 = true;
                  continue;
                }
                IN[2] = N + NR + 4 * NS + 5;
                DP[0][4] = DP[0][0] / DP[3][0] - DP[0][1] / DP[3][1];
                DP[1][4] = DP[1][0] / DP[3][0] - DP[1][1] / DP[3][1];
                DP[2][4] = DP[2][0] / DP[3][0] - DP[2][1] / DP[3][1];
                if (DP[0][4] * DP[0][4] <= DP[1][4] * DP[1][4] + DP[2][4] * DP[2][4]) DP[0][2] = 1.;
                if (DP[0][4] * DP[0][4] > DP[1][4] * DP[1][4] + DP[2][4] * DP[2][4]) DP[2][2] = 1.;
                if (DP[1][4] * DP[1][4] <= DP[0][4] * DP[0][4] + DP[2][4] * DP[2][4]) DP[1][3] = 1.;
                if (DP[1][4] * DP[1][4] > DP[0][4] * DP[0][4] + DP[2][4] * DP[2][4]) DP[2][3] = 1.;
                double DHCX1 = DFOUR(3, 1) / DHC12;
                double DHCX2 = DFOUR(3, 2) / DHC12;
                double DHCXX = 1. / std::sqrt(1. + 2. * DHCX1 * DHCX2 * DHC12);
                double DHCY1 = DFOUR(4, 1) / DHC12;
                double DHCY2 = DFOUR(4, 2) / DHC12;
                double DHCYX = DHCXX * (DHCX1 * DHCY2 + DHCX2 * DHCY1) * DHC12;
                double DHCYY = 1. / std::sqrt(1. + 2. * DHCY1 * DHCY2 * DHC12 - DHCYX * DHCYX);
                for (int J = 0; J < 4; ++J) {
                  DP[J][2] = DHCXX * (DP[J][2] - DHCX2 * DP[J][0] - DHCX1 * DP[J][1]);
                  P[J][IN[2] - 1] = DP[J][2];
                  P[J][IN[2]] =
                      DHCYY * (DP[J][3] - DHCY2 * DP[J][0] - DHCY1 * DP[J][1] - DHCYX * DP[J][2]);
                }

                // Express pT with respect to new axes, if sensible.
                double PXP = -(PX[2] * FOUR(IN[5], IN[2]) + PY[2] * FOUR(IN[5] + 1, IN[2]));
                double PYP = -(PX[2] * FOUR(IN[5], IN[2] + 1) + PY[2] * FOUR(IN[5] + 1, IN[2] + 1));
                if (std::abs(PXP * PXP + PYP * PYP - PX[2] * PX[2] - PY[2] * PY[2]) < 0.01) {
                  PX[2] = PXP;
                  PY[2] = PYP;
                }
              }

              // Junction strings: sum up known four - momentum, coefficients for m2.
              for (int J = 0; J < 4; ++J) {
                DHG[J] = 0.;
                P[J][I - 1] = PX[0] * P[J][IN[5] - 1] + PY[0] * P[J][IN[5]] +
                              PX[2] * P[J][IN[2] - 1] + PY[2] * P[J][IN[2]];
                for (int IN1 = IN[3]; IN1 < IN[0] - 4 + 1; IN1 += 4) {
                  P[J][I - 1] += P[2][IN1 + 1] * P[J][IN1 - 1];
                }
                for (int IN2 = IN[4]; IN2 < IN[1] - 4 + 1; IN2 += 4) {
                  P[J][I - 1] += P[2][IN2 + 1] * P[J][IN2 - 1];
                }
              }
              DHM[0] = FOUR(I, I);
              DHM[1] = 2. * FOUR(I, IN[0]);
              DHM[2] = 2. * FOUR(I, IN[1]);
              DHM[3] = 2. * FOUR(IN[0], IN[1]);

              // Junction strings: find coefficients for Gamma expression.
              for (int IN2 = IN[0] + 1; IN2 < IN[1] + 1; IN2 += 4) {
                for (int IN1 = IN[0]; IN1 < IN2; IN1 += 4) {
                  double DHC = 2. * FOUR(IN1, IN2);
                  DHG[0] += P[0][IN1 + 1] * P[0][IN2 + 1] * DHC;
                  if (IN1 == IN[0]) DHG[1] -= P[0][IN2 + 1] * DHC;
                  if (IN2 == IN[1]) DHG[2] += P[0][IN1 + 1] * DHC;
                  if (IN1 == IN[0] && IN2 == IN[1]) DHG[3] -= DHC;
                }
              }

              // Junction strings: solve (m2, Gamma) equation system for energies.
              double DHS1 = DHM[2] * DHG[3] - DHM[3] * DHG[2];
              if (std::abs(DHS1) < 1e-4) goto label_320;
              double DHS2 = DHM[3] * (GAM[2] - DHG[0]) - DHM[1] * DHG[2] -
                            DHG[3] * (P[4][I - 1] * P[4][I - 1] - DHM[0]) + DHG[1] * DHM[2];
              double DHS3 =
                  DHM[1] * (GAM[2] - DHG[0]) - DHG[1] * (P[4][I - 1] * P[4][I - 1] - DHM[0]);
              P[3][IN[1] + 1] =
                  0.5 * (std::sqrt(std::max(0., DHS2 * DHS2 - 4. * DHS1 * DHS3)) / std::abs(DHS1) -
                         DHS2 / DHS1);
              if (DHM[1] + DHM[3] * P[3][IN[1] + 1] <= 0.) goto label_320;
              P[3][IN[0] + 1] = (P[4][I - 1] * P[4][I - 1] - DHM[0] - DHM[2] * P[3][IN[1] + 1]) /
                                (DHM[1] + DHM[3] * P[3][IN[1] + 1]);

              // Junction strings: step to new region if necessary.
              if (P[3][IN[1] + 1] > P[2][IN[1] + 1]) {
                P[3][IN[1] + 1] = P[2][IN[1] + 1];
                P[0][IN[1] + 1] = 1.;
                IN[1] += 4;
                if (IN[1] > N + NR + 4 * NS) goto label_320;
                if (FOUR(IN[0], IN[1]) <= 1e-2) {
                  P[3][IN[0] + 1] = P[2][IN[0] + 1];
                  P[0][IN[0] + 1] = 0.;
                  IN[0] += 4;
                }
                repeat420 = true;
                continue;
              } else if (P[3][IN[0] + 1] > P[2][IN[0] + 1]) {
                P[3][IN[0] + 1] = P[2][IN[0] + 1];
                P[0][IN[0] + 1] = 0.;
                IN[0] += JS;
                goto label_820;
              }
            } while (repeat420);
          }  // skip condition 500
          goto500 = false;

          // Junction strings: particle four-momentum, remainder, loop back.
          for (int J = 0; J < 4; ++J) {
            P[J][I - 1] += P[3][IN[0] + 1] * P[J][IN[0] - 1] + P[3][IN[1] + 1] * P[J][IN[1] - 1];
            PJU[J][IUi + 1] += P[J][I - 1];
          }
          if (P[3][I - 1] < P[4][I - 1]) goto label_320;
          PJU[4][IUi + 2] = TJU[3] * PJU[3][IUi + 2] - TJU[0] * PJU[0][IUi + 2] -
                            TJU[1] * PJU[1][IUi + 2] - TJU[2] * PJU[2][IUi + 2];
          if (PJU[4][IUi + 2] < PJU[4][IUi - 1]) {
            KFL[0] = -KFL[2];
            PX[0] = -PX[2];
            PY[0] = -PY[2];
            GAM[0] = GAM[2];
            if (IN[2] != IN[5]) {
              for (int J = 0; J < 4; ++J) {
                P[J][IN[5] - 1] = P[J][IN[2] - 1];
                P[J][IN[5]] = P[J][IN[2]];
              }
            }
            for (int JQ = 0; JQ < 2; ++JQ) {
              IN[3 + JQ] = IN[JQ];
              P[2][IN[JQ] + 1] -= P[3][IN[JQ] + 1];
              P[0][IN[JQ] + 1] -= (3 - 2 * (JQ + 1)) * P[3][IN[JQ] + 1];
            }
            repeat380 = true;
            continue;
          }
        } while (repeat380);

        // Junction strings: save quantities left after each string.
        if (std::abs(KFL[0]) > 10) goto label_320;
        I--;
        KFJH[IUi] = KFL[0];
        for (int J = 0; J < 4; ++J) {
          PJU[J][IUi + 2] -= P[J][I];
        }
      }  // 550

      // Junction strings: put together to new effective string endpoint.
      NJS[JTi - 1] = I - ISTA;
      KFJS[JTi - 1] = K[1][K[2][MJU[JTi + 1] - 1] - 1];
      int KFLS = 2 * static_cast<int>(RLU() + 3. * PARJ[3] / (1. + 3. * PARJ[3])) + 1;
      if (KFJH[0] == KFJH[1]) KFLS = 3;
      int sgn = (KFJH[0] < 0) ? -1 : 1;
      if (ISTA != I)
        KFJS[JTi - 1] = sgn * (1000 * std::max(std::abs(KFJH[0]), std::abs(KFJH[1])) +
                               100 * std::min(std::abs(KFJH[0]), std::abs(KFJH[1])) + KFLS);
      for (int J = 0; J < 4; ++J) {
        PJS[J][JTi - 1] = PJU[J][0] + PJU[J][1] + P[J][MJU[JTi] - 1];
        PJS[J][JTi + 1] = PJU[J][3] + PJU[J][4];
      }
      PJS[4][JTi - 1] = std::sqrt(
          std::max(0., PJS[3][JTi - 1] * PJS[3][JTi - 1] - PJS[0][JTi - 1] * PJS[0][JTi - 1] -
                           PJS[1][JTi - 1] * PJS[1][JTi - 1] - PJS[2][JTi - 1] * PJS[2][JTi - 1]));
    }  // 570
  }    // 580

  // Open versus closed strings. Choose breakup region for latter.
  if (MJU[0] != 0 && MJU[1] != 0) {
    NS = MJU[1] - MJU[0];
    NB = MJU[0] - N;
  } else if (MJU[0] != 0) {
    NS = N + NR - MJU[0];
    NB = MJU[0] - N;
  } else if (MJU[1] != 0) {
    NS = MJU[1] - N;
    NB = 1;
  } else if (std::abs(K[1][N]) != 21) {
    NS = NR - 1;
    NB = 1;
  } else {
    NS = NR + 1;
    double W2SUM = 0.;
    for (int IS = 1; IS < NR + 1; ++IS) {
      P[0][N + NR + IS - 1] = 0.5 * FOUR(N + IS, N + IS + 1 - NR * (IS / NR));
      W2SUM += P[0][N + NR + IS - 1];
    }
    double W2RAN = RLU() * W2SUM;
    NB = 0;
    do {
      NB++;
      W2SUM -= P[0][N + NR + NB - 1];
    } while (W2SUM > W2RAN && NB < NR);
  }

  // Find longitudinal string directions (i.e. lightlike four-vectors).
  for (int IS = 1; IS < NS + 1; ++IS) {
    int IS1 = N + IS + NB - 1 - NR * ((IS + NB - 2) / NR);
    int IS2 = N + IS + NB - NR * ((IS + NB - 1) / NR);
    for (int J = 0; J < 5; ++J) {
      DP[J][0] = P[J][IS1 - 1];
      if (std::abs(K[1][IS1 - 1]) == 21) DP[J][0] *= 0.5;
      if (IS1 == MJU[0]) DP[J][0] = PJS[J][0] - PJS[J][2];
      DP[J][1] = P[J][IS2 - 1];
      if (std::abs(K[1][IS2 - 1]) == 21) DP[J][1] *= 0.5;
      if (IS2 == MJU[1]) DP[J][1] = PJS[J][1] - PJS[J][3];
    }
    DP[4][2] = DFOUR(1, 1);
    DP[4][3] = DFOUR(2, 2);
    double DHKC = DFOUR(1, 2);
    if (DP[4][2] + 2. * DHKC + DP[4][3] <= 0.) {
      DP[4][2] = DP[4][0] * DP[4][0];
      DP[4][3] = DP[4][1] * DP[4][1];
      DP[3][0] = std::sqrt(DP[0][0] * DP[0][0] + DP[1][0] * DP[1][0] + DP[2][0] * DP[2][0] +
                           DP[4][0] * DP[4][0]);
      DP[3][1] = std::sqrt(DP[0][1] * DP[0][1] + DP[1][1] * DP[1][1] + DP[2][1] * DP[2][1] +
                           DP[4][1] * DP[4][1]);
      DHKC = DFOUR(1, 2);
    }
    double DHKS = std::sqrt(DHKC * DHKC - DP[4][2] * DP[4][3]);
    double DHK1 = 0.5 * ((DP[4][3] + DHKC) / DHKS - 1.);
    double DHK2 = 0.5 * ((DP[4][2] + DHKC) / DHKS - 1.);
    int IN1 = N + NR + 4 * IS - 3;
    P[4][IN1 - 1] = std::sqrt(DP[4][2] + 2. * DHKC + DP[4][3]);
    for (int J = 0; J < 4; ++J) {
      P[J][IN1 - 1] = (1. + DHK1) * DP[J][0] - DHK2 * DP[J][1];
      P[J][IN1] = (1. + DHK2) * DP[J][1] - DHK1 * DP[J][0];
    }
  }

  // Begin initialization: sum up energy, set starting position.
  ISAV = I;
  MSTU91 = MSTU[89];

label_640:  // this is kind-of a do-while loop

  NTRY++;
  if (NTRY > 100 && NTRYR <= 4) {
    PARU12 *= 4.;
    PARU13 *= 2.;
    goto label_140;
  } else if (NTRY > 100) {
    LUERRM(14, "LUSTRF (7): caught in infinite loop");
    if (MSTU[20] >= 1) return;
  }
  I = ISAV;
  MSTU[89] = MSTU91;
  for (int J = 0; J < 4; ++J) {
    P[J][N + NRS - 1] = 0.;
    for (int IS = 1; IS < NR + 1; ++IS) {
      P[J][N + NRS - 1] += P[J][N + IS - 1];
    }
  }
  for (int JTi = 1; JTi < 3; ++JTi) {
    IRANK[JTi - 1] = 0;
    if (MJU[JTi - 1] != 0) IRANK[JTi - 1] = NJS[JTi - 1];
    if (NS > NR) IRANK[JTi - 1] = 1;
    IE[JTi - 1] = K[2][N + 1 + (JTi / 2) * (NP - 1) - 1];
    IN[3 * JTi] = N + NR + 1 + 4 * (JTi / 2) * (NS - 1);
    IN[3 * JTi + 1] = IN[3 * JTi] + 1;
    IN[3 * JTi + 2] = N + NR + 4 * NS + 2 * JTi - 1;
    for (int IN1 = N + NR + 2 + JTi; IN1 < N + NR + 4 * NS - 2 + JTi + 1; IN1 += 4) {
      P[0][IN1 - 1] = 2 - JTi;
      P[1][IN1 - 1] = JTi - 1;
      P[2][IN1 - 1] = 1.;
    }
  }

  // Initialize flavour and pT variables for open string.
  if (NS < NR) {
    PX[0] = 0.;
    PY[0] = 0.;
    if (NS == 1 && MJU[0] + MJU[1] == 0) {
      LUPTDI_output = LUPTDI(0);
      PX[0] = LUPTDI_output[0];
      PY[0] = LUPTDI_output[1];
    }
    PX[1] = -PX[0];
    PY[1] = -PY[0];
    for (int JTi = 1; JTi < 3; ++JTi) {
      KFL[JTi - 1] = K[1][IE[JTi - 1] - 1];
      if (MJU[JTi - 1] != 0) KFL[JTi - 1] = KFJS[JTi - 1];
      MSTJ[92] = 1;
      PMQ[JTi - 1] = ULMASS(KFL[JTi - 1]);
      GAM[JTi - 1] = 0.;
    }

    // Closed string: random initial breakup flavour, pT and vertex.
  } else {
    KFL[2] =
        static_cast<int>(1. + (2. + PARJ[1]) * RLU()) * std::pow(-1, static_cast<int>(RLU() + 0.5));
    LUKFDI_output = LUKFDI(KFL[2], 0);
    KFL[0] = LUKFDI_output[0];
    KFL[1] = -KFL[0];
    if (std::abs(KFL[0]) > 10 && RLU() > 0.5) {
      int sgn = (KFL[0] < 0) ? -1 : 1;
      KFL[1] = -(KFL[0] + 10000 * sgn);
    } else if (std::abs(KFL[0]) > 10) {
      int sgn = (KFL[1] < 0) ? -1 : 1;
      KFL[0] = -(KFL[1] + 10000 * sgn);
    }
    std::vector<double> LUPTDI_output = LUPTDI(KFL[0]);
    PX[0] = LUPTDI_output[0];
    PY[0] = LUPTDI_output[1];
    PX[1] = -PX[0];
    PY[1] = -PY[0];
    double PR3 = std::min(25., 0.1 * P[4][N + NR] * P[4][N + NR]);
    double ZR = 0.;
    do {
      Z = LUZDIS(KFL[0], KFL[1], PR3);
      ZR = PR3 / (Z * P[4][N + NR] * P[4][N + NR]);
    } while (ZR >= 1.);
    for (int JTi = 1; JTi < 3; ++JTi) {
      MSTJ[92] = 1;
      PMQ[JTi - 1] = ULMASS(KFL[JTi - 1]);
      GAM[JTi - 1] = PR3 * (1. - Z) / Z;
      int IN1 = N + NR + 3 + 4 * (JTi / 2) * (NS - 1);
      P[JTi - 1][IN1 - 1] = 1. - Z;
      P[2 - JTi][IN1 - 1] = JTi - 1;
      P[2][IN1 - 1] = (2 - JTi) * (1. - Z) + (JTi - 1) * Z;
      P[JTi - 1][IN1] = ZR;
      P[2 - JTi][IN1] = 2 - JTi;
      P[2][IN1] = (2 - JTi) * (1. - ZR) + (JTi - 1) * ZR;
    }
  }

  // Find initial transverse directions (i.e. spacelike four-vectors).
  for (int JTi = 1; JTi < 3; ++JTi) {
    int IN3 = 0;
    if (JTi == 1 || NS == NR - 1) {
      int IN1 = IN[3 * JTi];
      IN3 = IN[3 * JTi + 2];
      for (int J = 0; J < 4; ++J) {
        DP[J][0] = P[J][IN1 - 1];
        DP[J][1] = P[J][IN1];
        DP[J][2] = 0.;
        DP[J][3] = 0.;
      }
      DP[3][0] = std::sqrt(DP[0][0] * DP[0][0] + DP[1][0] * DP[1][0] + DP[2][0] * DP[2][0]);
      DP[3][1] = std::sqrt(DP[0][1] * DP[0][1] + DP[1][1] * DP[1][1] + DP[2][1] * DP[2][1]);
      DP[0][4] = DP[0][0] / DP[3][0] - DP[0][1] / DP[3][1];
      DP[1][4] = DP[1][0] / DP[3][0] - DP[1][1] / DP[3][1];
      DP[2][4] = DP[2][0] / DP[3][0] - DP[2][1] / DP[3][1];
      if (DP[0][4] * DP[0][4] <= DP[1][4] * DP[1][4] + DP[2][4] * DP[2][4]) DP[0][2] = 1.;
      if (DP[0][4] * DP[0][4] > DP[1][4] * DP[1][4] + DP[2][4] * DP[2][4]) DP[2][2] = 1.;
      if (DP[1][4] * DP[1][4] <= DP[0][4] * DP[0][4] + DP[2][4] * DP[2][4]) DP[1][3] = 1.;
      if (DP[1][4] * DP[1][4] > DP[0][4] * DP[0][4] + DP[2][4] * DP[2][4]) DP[2][3] = 1.;
      double DHC12 = DFOUR(1, 2);
      double DHCX1 = DFOUR(3, 1) / DHC12;
      double DHCX2 = DFOUR(3, 2) / DHC12;
      double DHCXX = 1. / std::sqrt(1. + 2. * DHCX1 * DHCX2 * DHC12);
      double DHCY1 = DFOUR(4, 1) / DHC12;
      double DHCY2 = DFOUR(4, 2) / DHC12;
      double DHCYX = DHCXX * (DHCX1 * DHCY2 + DHCX2 * DHCY1) * DHC12;
      double DHCYY = 1. / std::sqrt(1. + 2. * DHCY1 * DHCY2 * DHC12 - DHCYX * DHCYX);
      for (int J = 0; J < 4; ++J) {
        DP[J][2] = DHCXX * (DP[J][2] - DHCX2 * DP[J][0] - DHCX1 * DP[J][1]);
        P[J][IN3 - 1] = DP[J][2];
        P[J][IN3] = DHCYY * (DP[J][3] - DHCY2 * DP[J][0] - DHCY1 * DP[J][1] - DHCYX * DP[J][2]);
      }
    } else {
      for (int J = 0; J < 4; ++J) {
        P[J][IN3 + 1] = P[J][IN3 - 1];
        P[J][IN3 + 2] = P[J][IN3];
      }
    }
  }

  // Remove energy used up in junction string fragmentation.
  if (MJU[0] + MJU[1] > 0) {
    for (int JTi = 1; JTi < 3; ++JTi) {
      if (NJS[JTi - 1] == 0) continue;
      for (int J = 0; J < 4; ++J) {
        P[J][N + NRS - 1] -= PJS[J][JTi + 1];
      }
    }
  }

  // Produce new particle: side, origin.
  do {  // 780
    I++;
    if (2 * I - NSAV >= MSTU[3] - MSTU[31] - 5) {
      LUERRM(11, "LUSTRF in loop 780: no more memory left in LUJETS");
      if (MSTU[20] >= 1) return;
    }

    JT = static_cast<int>(1.5 + RLU());
    if (std::abs(KFL[2 - JT]) > 10) JT = 3 - JT;
    if (std::abs(KFL[2 - JT]) >= 4 && std::abs(KFL[2 - JT]) <= 8) JT = 3 - JT;
    JR = 3 - JT;
    JS = 3 - 2 * JT;
    IRANK[JT - 1]++;
    K[0][I - 1] = 1;
    K[2][I - 1] = IE[JT - 1];
    K[3][I - 1] = 0;
    K[4][I - 1] = 0;

    // Generate flavour, hadron and pT.
    do {  // 790
      std::vector<int> LUKFDI_output = LUKFDI(KFL[JT - 1], 0);
      KFL[2] = LUKFDI_output[0];
      K[1][I - 1] = LUKFDI_output[1];
      if (K[1][I - 1] == 0) goto label_640;
    } while ((MSTJ[11] >= 3 && IRANK[JT - 1] == 1 && std::abs(KFL[JT - 1]) <= 10 &&
              std::abs(KFL[2]) > 10) &&
             RLU() > PARJ[18]);
    P[4][I - 1] = ULMASS(K[1][I - 1]);
    LUPTDI_output = LUPTDI(KFL[JT - 1]);
    PX[2] = LUPTDI_output[0];
    PY[2] = LUPTDI_output[1];
    PR[JT - 1] = P[4][I - 1] * P[4][I - 1] + (PX[JT - 1] + PX[2]) * (PX[JT - 1] + PX[2]) +
                 (PY[JT - 1] + PY[2]) * (PY[JT - 1] + PY[2]);

    // Final hadrons for small invariant mass.
    MSTJ[92] = 1;
    PMQ[2] = ULMASS(KFL[2]);
    PARJST = PARJ[32];
    if (MSTJ[10] == 2) PARJST = PARJ[33];
    WMIN = PARJST + PMQ[0] + PMQ[1] + PARJ[35] * PMQ[2];
    if (std::abs(KFL[JT - 1]) > 10 && std::abs(KFL[2]) > 10) WMIN -= 0.5 * PARJ[35] * PMQ[2];
    WREM2 = FOUR(N + NRS, N + NRS);
    if (WREM2 < 0.10) goto label_640;
    if (WREM2 <
        std::pow(std::max(WMIN * (1. + (2. * RLU() - 1.) * PARJ[36]), PARJ[31] + PMQ[0] + PMQ[1]),
                 2)) {
      break;  // leave 780 and goto point 940
    }

    // Choose z, which gives Gamma. Shift z for heavy flavours.
    Z = LUZDIS(KFL[JT - 1], KFL[2], PR[JT - 1]);
    if (std::abs(KFL[JT - 1]) >= 4 && std::abs(KFL[JT - 1]) <= 8 && MSTU[89] < 8) {
      MSTU[89]++;
      MSTU[89 + MSTU[89]] = I;
      PARU[89 + MSTU[89]] = Z;
    }
    KFL1A = std::abs(KFL[0]);
    KFL2A = std::abs(KFL[1]);
    if (std::max({KFL1A % 10, (KFL1A / 1000) % 10, KFL2A % 10, (KFL2A / 1000) % 10}) >= 4) {
      PR[JR - 1] = (PMQ[JR - 1] + PMQ[2]) * (PMQ[JR - 1] + PMQ[2]) +
                   (PX[JR - 1] - PX[2]) * (PX[JR - 1] - PX[2]) +
                   (PY[JR - 1] - PY[2]) * (PY[JR - 1] - PY[2]);
      double PW12 = std::sqrt(
          std::max(0., (WREM2 - PR[0] - PR[1]) * (WREM2 - PR[0] - PR[1]) - 4. * PR[0] * PR[1]));
      Z = (WREM2 + PR[JT - 1] - PR[JR - 1] + PW12 * (2. * Z - 1.)) / (2. * WREM2);
      PR[JR - 1] = (PMQ[JR - 1] + PARJST) * (PMQ[JR - 1] + PARJST) +
                   (PX[JR - 1] - PX[2]) * (PX[JR - 1] - PX[2]) +
                   (PY[JR - 1] - PY[2]) * (PY[JR - 1] - PY[2]);
      if ((1. - Z) * (WREM2 - PR[JT - 1] / Z) < PR[JR - 1]) break;  // leave 780 and goto point 940
    }
    GAM[2] = (1. - Z) * (GAM[JT - 1] + PR[JT - 1] / Z);
    for (int J = 0; J < 3; ++J) {
      IN[J] = IN[3 * JT + J];
    }

    goto900 = false;  // skip condition 900
    // Stepping within or from "low" string region easy.
    if (IN[0] + 1 == IN[1] &&
        Z * P[2][IN[0] + 1] * P[2][IN[1] + 1] * P[4][IN[0] - 1] * P[4][IN[0] - 1] >= PR[JT - 1]) {
      P[3][IN[JT - 1] + 1] = Z * P[2][IN[JT - 1] + 1];
      P[3][IN[JR - 1] + 1] =
          PR[JT - 1] / (P[3][IN[JT - 1] + 1] * P[4][IN[0] - 1] * P[4][IN[0] - 1]);
      for (int J = 0; J < 4; ++J) {
        P[J][I - 1] = (PX[JT - 1] + PX[2]) * P[J][IN[2] - 1] + (PY[JT - 1] + PY[2]) * P[J][IN[2]];
      }
      goto900 = true;  // skip condition 900
    } else if (IN[0] + 1 == IN[1] && goto900 == false) {
      P[3][IN[JR - 1] + 1] = P[2][IN[JR - 1] + 1];
      P[JT - 1][IN[JR - 1] + 1] = 1.;
      IN[JR - 1] += 4 * JS;
      if (JS * IN[JR - 1] > JS * IN[4 * JR - 1]) goto label_640;
      if (FOUR(IN[0], IN[1]) <= 1e-2) {
        P[3][IN[JT - 1] + 1] = P[2][IN[JT - 1] + 1];
        P[JT - 1][IN[JT - 1] + 1] = 0.;
        IN[JT - 1] += 4 * JS;
      }
    }

    if (goto900 == false) {
    // Find new transverse directions (i.e. spacelike string vectors).
    label_820:

      bool repeat820 = false;
      do {  // 820
        if (JS * IN[0] > JS * IN[3 * JR] || JS * IN[1] > JS * IN[3 * JR + 1] || IN[0] > IN[1])
          goto label_640;
        if (IN[0] != IN[3 * JT] || IN[1] != IN[3 * JT + 1]) {
          for (int J = 0; J < 4; ++J) {
            DP[J][0] = P[J][IN[0] - 1];
            DP[J][1] = P[J][IN[1] - 1];
            DP[J][2] = 0.;
            DP[J][3] = 0.;
          }
          DP[3][0] = std::sqrt(DP[0][0] * DP[0][0] + DP[1][0] * DP[1][0] + DP[2][0] * DP[2][0]);
          DP[3][1] = std::sqrt(DP[0][1] * DP[0][1] + DP[1][1] * DP[1][1] + DP[2][1] * DP[2][1]);
          double DHC12 = DFOUR(1, 2);
          if (DHC12 <= 1e-2) {
            P[3][IN[JT - 1] + 1] = P[2][IN[JT - 1] + 1];
            P[JT - 1][IN[JT - 1] + 1] = 0.;
            IN[JT - 1] += 4 * JS;
            repeat820 = true;
            continue;
          }
          IN[2] = N + NR + 4 * NS + 5;
          DP[0][4] = DP[0][0] / DP[3][0] - DP[0][1] / DP[3][1];
          DP[1][4] = DP[1][0] / DP[3][0] - DP[1][1] / DP[3][1];
          DP[2][4] = DP[2][0] / DP[3][0] - DP[2][1] / DP[3][1];
          if (DP[0][4] * DP[0][4] <= DP[1][4] * DP[1][4] + DP[2][4] * DP[2][4]) DP[0][2] = 1.;
          if (DP[0][4] * DP[0][4] > DP[1][4] * DP[1][4] + DP[2][4] * DP[2][4]) DP[2][2] = 1.;
          if (DP[1][4] * DP[1][4] <= DP[0][4] * DP[0][4] + DP[2][4] * DP[2][4]) DP[1][3] = 1.;
          if (DP[1][4] * DP[1][4] > DP[0][4] * DP[0][4] + DP[2][4] * DP[2][4]) DP[2][3] = 1.;
          double DHCX1 = DFOUR(3, 1) / DHC12;
          double DHCX2 = DFOUR(3, 2) / DHC12;
          double DHCXX = 1. / std::sqrt(1. + 2. * DHCX1 * DHCX2 * DHC12);
          double DHCY1 = DFOUR(4, 1) / DHC12;
          double DHCY2 = DFOUR(4, 2) / DHC12;
          double DHCYX = DHCXX * (DHCX1 * DHCY2 + DHCX2 * DHCY1) * DHC12;
          double DHCYY = 1. / std::sqrt(1. + 2. * DHCY1 * DHCY2 * DHC12 - DHCYX * DHCYX);
          for (int J = 0; J < 4; ++J) {
            DP[J][2] = DHCXX * (DP[J][2] - DHCX2 * DP[J][0] - DHCX1 * DP[J][1]);
            P[J][IN[2] - 1] = DP[J][2];
            P[J][IN[2]] =
                DHCYY * (DP[J][3] - DHCY2 * DP[J][0] - DHCY1 * DP[J][1] - DHCYX * DP[J][2]);
          }

          // Express pT with respect to new axes, if sensible.
          double PXP =
              -(PX[2] * FOUR(IN[3 * JT + 2], IN[2]) + PY[2] * FOUR(IN[3 * JT + 2] + 1, IN[2]));
          double PYP = -(PX[2] * FOUR(IN[3 * JT + 2], IN[2] + 1) +
                         PY[2] * FOUR(IN[3 * JT + 2] + 1, IN[2] + 1));
          if (std::abs(PXP * PXP + PYP * PYP - PX[2] * PX[2] - PY[2] * PY[2]) < 0.01) {
            PX[2] = PXP;
            PY[2] = PYP;
          }
        }

        // Sum up known four-momentum. Gives coefficients for m2 expression.
        for (int J = 0; J < 4; ++J) {
          DHG[J] = 0.;
          P[J][I - 1] = PX[JT - 1] * P[J][IN[3 * JT + 2] - 1] + PY[JT - 1] * P[J][IN[3 * JT + 2]] +
                        PX[2] * P[J][IN[2] - 1] + PY[2] * P[J][IN[2]];
          for (int IN1 = IN[3 * JT]; IN1 < IN[0] - 4 * JS + 1; IN1 += 4 * JS) {
            P[J][I - 1] += P[2][IN1 + 1] * P[J][IN1 - 1];
          }
          for (int IN2 = IN[3 * JT + 1]; IN2 < IN[1] - 4 * JS + 1; IN2 += 4 * JS) {
            P[J][I - 1] += P[2][IN2 + 1] * P[J][IN2 - 1];
          }
        }
        DHM[0] = FOUR(I, I);
        DHM[1] = 2. * FOUR(I, IN[0]);
        DHM[2] = 2. * FOUR(I, IN[1]);
        DHM[3] = 2. * FOUR(IN[0], IN[1]);

        // Find coefficients for Gamma expression.
        for (int IN2 = IN[0] + 1; IN2 < IN[1] + 1; IN2 += 4) {
          for (int IN1 = IN[0]; IN1 < IN2; IN1 += 4) {
            double DHC = 2. * FOUR(IN1, IN2);
            DHG[0] += P[JT - 1][IN1 + 1] * P[JT - 1][IN2 + 1] * DHC;
            if (IN1 == IN[0]) DHG[1] -= JS * P[JT - 1][IN2 + 1] * DHC;
            if (IN2 == IN[1]) DHG[2] += JS * P[JT - 1][IN1 + 1] * DHC;
            if (IN1 == IN[0] && IN2 == IN[1]) DHG[3] -= DHC;
          }
        }

        // Solve (m2, Gamma) equation system for energies taken.
        double DHS1 = DHM[JR] * DHG[3] - DHM[3] * DHG[JR];
        if (std::abs(DHS1) < 1e-4) goto label_640;
        double DHS2 = DHM[3] * (GAM[2] - DHG[0]) - DHM[JT] * DHG[JR] -
                      DHG[3] * (P[4][I - 1] * P[4][I - 1] - DHM[0]) + DHG[JT] * DHM[JR];
        double DHS3 = DHM[JT] * (GAM[2] - DHG[0]) - DHG[JT] * (P[4][I - 1] * P[4][I - 1] - DHM[0]);
        P[3][IN[JR - 1] + 1] =
            0.5 * (std::sqrt(std::max(0., DHS2 * DHS2 - 4. * DHS1 * DHS3)) / std::abs(DHS1) -
                   DHS2 / DHS1);
        if (DHM[JT] + DHM[3] * P[3][IN[JR - 1] + 1] <= 0.) goto label_640;
        P[3][IN[JT - 1] + 1] =
            (P[4][I - 1] * P[4][I - 1] - DHM[0] - DHM[JR] * P[3][IN[JR - 1] + 1]) /
            (DHM[JT] + DHM[3] * P[3][IN[JR - 1] + 1]);

        // Step to new region if necessary.
        if (P[3][IN[JR - 1] + 1] > P[2][IN[JR - 1] + 1]) {
          P[3][IN[JR - 1] + 1] = P[2][IN[JR - 1] + 1];
          P[JT - 1][IN[JR - 1] + 1] = 1.;
          IN[JR - 1] += 4 * JS;
          if (JS * IN[JR - 1] > JS * IN[4 * JR - 1]) goto label_640;
          if (FOUR(IN[0], IN[1]) <= 1e-2) {
            P[3][IN[JT - 1] + 1] = P[2][IN[JT - 1] + 1];
            P[JT - 1][IN[JT - 1] + 1] = 0.;
            IN[JT - 1] += 4 * JS;
          }
          repeat820 = true;
          continue;
        } else if (P[3][IN[JT - 1] + 1] > P[2][IN[JT - 1] + 1]) {
          P[3][IN[JT - 1] + 1] = P[2][IN[JT - 1] + 1];
          P[JT - 1][IN[JT - 1] + 1] = 0.;
          IN[JT - 1] += 4 * JS;
          repeat820 = true;
          continue;
        }
      } while (repeat820);
    }  // skip condition 900
    goto900 = false;

    // Four-momentum of particle. Remaining quantities. Loop back.
    for (int J = 0; J < 4; ++J) {
      P[J][I - 1] += P[3][IN[0] + 1] * P[J][IN[0] - 1] + P[3][IN[1] + 1] * P[J][IN[1] - 1];
      P[J][N + NRS - 1] -= P[J][I - 1];
    }

    if (P[3][I - 1] < P[4][I - 1]) goto label_640;
    KFL[JT - 1] = -KFL[2];
    PMQ[JT - 1] = PMQ[2];
    PX[JT - 1] = -PX[2];
    PY[JT - 1] = -PY[2];
    GAM[JT - 1] = GAM[2];
    if (IN[2] != IN[3 * JT + 2]) {
      for (int J = 0; J < 4; ++J) {
        P[J][IN[3 * JT + 2] - 1] = P[J][IN[2] - 1];
        P[J][IN[3 * JT + 2]] = P[J][IN[2]];
      }
    }
    for (int JQ = 0; JQ < 2; ++JQ) {
      IN[3 * JT + JQ] = IN[JQ];
      P[2][IN[JQ] + 1] -= P[3][IN[JQ] + 1];
      P[JT - 1][IN[JQ] + 1] -= JS * (3 - 2 * (JQ + 1)) * P[3][IN[JQ] + 1];
    }
  } while (true);  // 780 - this actually seems to be unconditional. goto label_640 and 940 leave
                   // this loop.

  // goto 940 point

  // Final hadron: side, flavour, hadron, mass.
  I++;
  K[0][I - 1] = 1;
  K[2][I - 1] = IE[JR - 1];
  K[3][I - 1] = 0;
  K[4][I - 1] = 0;
  LUKFDI_output = LUKFDI(KFL[JR - 1], -KFL[2]);
  K[1][I - 1] = LUKFDI_output[1];
  if (K[1][I - 1] == 0) goto label_640;
  P[4][I - 1] = ULMASS(K[1][I - 1]);
  PR[JR - 1] = P[4][I - 1] * P[4][I - 1] + (PX[JR - 1] - PX[2]) * (PX[JR - 1] - PX[2]) +
               (PY[JR - 1] - PY[2]) * (PY[JR - 1] - PY[2]);

  // Final two hadrons: find common setup of four-vectors.
  int JQ = 1;
  if (P[2][IN[3] + 1] * P[2][IN[4] + 1] * FOUR(IN[3], IN[4]) <
      P[2][IN[6] - 1] * P[2][IN[7] - 1] * FOUR(IN[6], IN[7]))
    JQ = 2;
  double DHC12 = FOUR(IN[3 * JQ], IN[3 * JQ + 1]);
  double DHR1 = FOUR(N + NRS, IN[3 * JQ + 1]) / DHC12;
  double DHR2 = FOUR(N + NRS, IN[3 * JQ]) / DHC12;
  if (IN[3] != IN[6] || IN[4] != IN[7]) {
    PX[2 - JQ] = -FOUR(N + NRS, IN[3 * JQ + 2] - 1) - PX[JQ - 1];
    PY[2 - JQ] = -FOUR(N + NRS, IN[3 * JQ + 2]) - PY[JQ - 1];
    PR[2 - JQ] = std::pow(P[4][I + (JT + JQ - 3) * (JT + JQ - 3) - 2], 2) +
                 std::pow(PX[2 - JQ] + (2 * JQ - 3) * JS * PX[2], 2) +
                 std::pow(PY[2 - JQ] + (2 * JQ - 3) * JS * PY[2], 2);
  }

  // Solve kinematics for final two hadrons, if possible.
  WREM2 += (PX[0] + PX[1]) * (PX[0] + PX[1]) + (PY[0] + PY[1]) * (PY[0] + PY[1]);
  double FD = (std::sqrt(PR[0]) + std::sqrt(PR[1])) / std::sqrt(WREM2);
  if (MJU[0] + MJU[1] != 0 && I == ISAV + 2 && FD >= 1.) goto label_200;
  if (FD >= 1.) goto label_640;
  double FA = WREM2 + PR[JT - 1] - PR[JR - 1];
  double PREV = 0.;
  if (MSTJ[10] != 2)
    PREV =
        0.5 * std::exp(std::max(-50., std::log(FD) * PARJ[37] * (PR[0] + PR[1]) * (PR[0] + PR[1])));
  if (MSTJ[10] == 2) PREV = 0.5 * std::pow(FD, PARJ[38]);
  int sgn = (JS * (RLU() - PREV) < 0) ? -1 : 1;
  double FB = sgn * (std::sqrt(std::max(0., FA * FA - 4. * WREM2 * PR[JT - 1])));
  KFL1A = std::abs(KFL[0]);
  KFL2A = std::abs(KFL[1]);
  if (std::max({KFL1A % 10, (KFL1A / 1000) % 10, KFL2A % 10, (KFL2A / 1000) % 10}) >= 6) {
    sgn = (JS < 0) ? -1 : 1;
    FB = sgn * (std::sqrt(std::max(0., FA * FA - 4. * WREM2 * PR[JT - 1])));
  }
  for (int J = 0; J < 4; ++J) {
    P[J][I - 2] = (PX[JT - 1] + PX[2]) * P[J][IN[3 * JQ + 2] - 1] +
                  (PY[JT - 1] + PY[2]) * P[J][IN[3 * JQ + 2]] +
                  0.5 *
                      (DHR1 * (FA + FB) * P[J][IN[3 * JQ] - 1] +
                       DHR2 * (FA - FB) * P[J][IN[3 * JQ + 1] - 1]) /
                      WREM2;
    P[J][I - 1] = P[J][N + NRS - 1] - P[J][I - 2];
  }
  if (P[3][I - 2] < P[4][I - 2] || P[3][I - 1] < P[4][I - 1]) goto label_640;

  // Mark jets as fragmented and give daughter pointers.
  N = I - NRS + 1;
  for (int Ii = NSAV; Ii < NSAV + NP; ++Ii) {
    int IM = K[2][Ii];
    K[0][IM - 1] += 10;
    if (MSTU[15] != 2) {
      K[3][IM - 1] = NSAV + 1;
      K[4][IM - 1] = NSAV + 1;
    } else {
      K[3][IM - 1] = NSAV + 2;
      K[4][IM - 1] = N;
    }
  }

  // Document string system. Move up particles.
  NSAV++;
  K[0][NSAV - 1] = 11;
  K[1][NSAV - 1] = 92;
  K[2][NSAV - 1] = IP;
  K[3][NSAV - 1] = NSAV + 1;
  K[4][NSAV - 1] = N;
  for (int J = 0; J < 4; ++J) {
    P[J][NSAV - 1] = DPS[J];
    V[J][NSAV - 1] = V[J][IP - 1];
  }
  P[4][NSAV - 1] = std::sqrt(
      std::max(0., DPS[3] * DPS[3] - DPS[0] * DPS[0] - DPS[1] * DPS[1] - DPS[2] * DPS[2]));
  V[4][NSAV - 1] = 0.;
  for (int Ii = NSAV; Ii < N; ++Ii) {
    for (int J = 0; J < 5; ++J) {
      K[J][Ii] = K[J][Ii + NRS - 1];
      P[J][Ii] = P[J][Ii + NRS - 1];
      V[J][Ii] = 0.;
    }
  }
  MSTU91 = MSTU[89];
  for (int IZ = MSTU90; IZ < MSTU91; ++IZ) {
    MSTU9T[IZ] = MSTU[90 + IZ] - NRS + 1 - NSAV + N;
    PARU9T[IZ] = PARU[90 + IZ];
  }
  MSTU[89] = MSTU90;

  // Order particles in rank along the chain. Update mother pointer.
  for (int Ii = NSAV; Ii < N; ++Ii) {
    for (int J = 0; J < 5; ++J) {
      K[J][Ii - NSAV + N] = K[J][Ii];
      P[J][Ii - NSAV + N] = P[J][Ii];
    }
  }
  int I1 = NSAV;
  for (int Ii = N; Ii < 2 * N - NSAV; ++Ii) {
    if (K[2][Ii] != IE[0]) continue;
    I1++;
    for (int J = 0; J < 5; ++J) {
      K[J][I1 - 1] = K[J][Ii];
      P[J][I1 - 1] = P[J][Ii];
    }
    if (MSTU[15] != 2) K[2][I1 - 1] = NSAV;
    for (int IZ = MSTU90; IZ < MSTU91; ++IZ) {
      if (MSTU9T[IZ] == Ii + 1) {
        MSTU[89]++;
        MSTU[89 + MSTU[89]] = I1;
        PARU[89 + MSTU[89]] = PARU9T[IZ];
      }
    }
  }
  for (int Ii = 2 * N - NSAV; Ii > N; --Ii) {
    if (K[2][Ii - 1] == IE[0]) continue;
    I1++;
    for (int J = 0; J < 5; ++J) {
      K[J][I1 - 1] = K[J][Ii - 1];
      P[J][I1 - 1] = P[J][Ii - 1];
    }
    if (MSTU[15] != 2) K[2][I1 - 1] = NSAV;
    for (int IZ = MSTU90; IZ < MSTU91; ++IZ) {
      if (MSTU9T[IZ] == Ii) {
        MSTU[89]++;
        MSTU[89 + MSTU[89]] = I1;
        PARU[89 + MSTU[89]] = PARU9T[IZ];
      }
    }
  }

  // Boost back particle system. Set production vertices.
  if (MBST == 0) {
    MSTU[32] = 1;
    LUDBRB(NSAV + 1, N, 0., 0., DPS[0] / DPS[3], DPS[1] / DPS[3], DPS[2] / DPS[3]);
  } else {
    for (int Ii = NSAV; Ii < N; ++Ii) {
      double HHPMT = P[0][Ii] * P[0][Ii] + P[1][Ii] * P[1][Ii] + P[4][Ii] * P[4][Ii];
      if (P[2][Ii] > 0.) {
        double HHPEZ = (P[3][Ii] + P[2][Ii]) * HHBZ;
        P[2][Ii] = 0.5 * (HHPEZ - HHPMT / HHPEZ);
        P[3][Ii] = 0.5 * (HHPEZ + HHPMT / HHPEZ);
      } else {
        double HHPEZ = (P[3][Ii] - P[2][Ii]) / HHBZ;
        P[2][Ii] = -0.5 * (HHPEZ - HHPMT / HHPEZ);
        P[3][Ii] = 0.5 * (HHPEZ + HHPMT / HHPEZ);
      }
    }
  }
  for (int Ii = NSAV; Ii < N; ++Ii) {
    for (int J = 0; J < 4; ++J) {
      V[J][Ii] = V[J][IP - 1];
    }
  }

  return;
}

void sophia_interface::LUINDF(int IP) {
  // Purpose: to handle the fragmentation of a jet system (or a single jet)
  // according to independent fragmentation models.
  double DPS[5] = {0.};
  double PSI[4] = {0.};
  int NFI[3] = {0};
  int NFL[3] = {0};
  int IFET[3] = {0};
  int KFLF[3] = {0};
  int KFLO[2] = {0};
  double PXO[2] = {0.};
  double PYO[2] = {0.};
  double WO[2] = {0.};

  // Reset counters. Identify parton system and take copy. Check flavour.
  int NSAV = N;
  int MSTU90 = MSTU[89];
  int NJET = 0;
  int KQSUM = 0;
  int I = IP - 1;
  bool repeat110 = false;
  do {  // 110
    repeat110 = false;
    I++;
    if (I > std::min(N, MSTU[3] - MSTU[31])) {
      LUERRM(12, "(LUINDF:) failed to reconstruct jet system");
      if (MSTU[20] >= 1) return;
    }
    if (K[0][I - 1] != 1 && K[0][I - 1] != 2) {
      repeat110 = true;
      continue;
    }
    int KC = LUCOMP(K[1][I - 1]);
    if (KC == 0) {
      repeat110 = true;
      continue;
    }
    int sgn = (K[1][I - 1] < 0) ? -1 : 1;
    int KQ = KCHG[1][KC - 1] * sgn;
    if (KQ == 0) {
      repeat110 = true;
      continue;
    }
    NJET++;
    if (KQ != 2) KQSUM += KQ;
    for (int J = 0; J < 5; ++J) {
      K[J][NSAV + NJET - 1] = K[J][I - 1];
      P[J][NSAV + NJET - 1] = P[J][I - 1];
      DPS[J] += P[J][I - 1];
    }
    K[2][NSAV + NJET - 1] = I;
    if (K[0][I - 1] == 2 || (MSTJ[2] <= 5 && N > I && K[0][I] == 2)) {
      repeat110 = true;
      continue;
    }
  } while (repeat110);
  if (NJET != 1 && KQSUM != 0) {
    LUERRM(12, "(LUINDF:) unphysical flavour combination");
    if (MSTU[20] >= 1) return;
  }

  // Boost copied system to CM frame. Find CM energy and sum flavours.
  if (NJET != 1) {
    MSTU[32] = 1;
    LUDBRB(NSAV + 1, NSAV + NJET, 0., 0., -DPS[0] / DPS[3], -DPS[1] / DPS[3], -DPS[2] / DPS[3]);
  }
  double PECM = 0.;
  for (int J = 0; J < 3; ++J) {
    NFI[J] = 0;
  }
  for (int Ii = NSAV; Ii < NSAV + NJET; ++Ii) {
    PECM += P[3][Ii];
    int KFA = std::abs(K[1][Ii]);
    int sgn = (K[1][Ii] < 0) ? -1 : 1;
    if (KFA <= 3) {
      NFI[KFA - 1] += sgn;
    } else if (KFA > 1000) {
      int KFLA = (KFA / 1000) % 10;
      int KFLB = (KFA / 100) % 10;
      if (KFLA <= 3) NFI[KFLA - 1] += sgn;
      if (KFLB <= 3) NFI[KFLB - 1] += sgn;
    }
  }

  // Loop over attempts made. Reset counters.
  int NTRY = 0;
  bool repeat150 = false;
  do {  // 150
    repeat150 = false;
    NTRY++;
    if (NTRY > 200) {
      LUERRM(14, "(LUINDF:) caught in infinite loop");
      if (MSTU[20] >= 1) return;
    }
    N = NSAV + NJET;
    MSTU[89] = MSTU90;
    for (int J = 0; J < 3; ++J) {
      NFL[J] = NFI[J];
      IFET[J] = 0;
      KFLF[J] = 0;
    }

    // Loop over jets to be fragmented.
    for (int IP1 = NSAV + 1; IP1 < NSAV + NJET + 1; ++IP1) {  // 230
      MSTJ[90] = 0;
      int NSAV1 = N;
      int MSTU91 = MSTU[89];

      // Initial flavour and momentum values. Jet along  + z axis.
      int KFLH = std::abs(K[1][IP1 - 1]);
      if (KFLH > 10) KFLH = (KFLH / 1000) % 10;
      KFLO[1] = 0;
      double WF =
          P[3][IP1 - 1] + std::sqrt(P[0][IP1 - 1] * P[0][IP1 - 1] + P[1][IP1 - 1] * P[1][IP1 - 1] +
                                    P[2][IP1 - 1] * P[2][IP1 - 1]);

      // Initial values for quark or diquark jet.
      do {  // 170
        int NSTR = 0;
        if (std::abs(K[1][IP1 - 1]) != 21) {
          NSTR = 1;
          KFLO[0] = K[1][IP1 - 1];
          std::vector<double> LUPTDI_output = LUPTDI(0);
          PXO[0] = LUPTDI_output[0];
          PYO[0] = LUPTDI_output[1];
          WO[0] = WF;

          // Initial values for gluon treated like random quark jet.
        } else if (MSTJ[1] <= 2) {
          NSTR = 1;
          if (MSTJ[1] == 2) MSTJ[90] = 1;
          KFLO[0] = static_cast<int>(1. + (2. + PARJ[1]) * RLU()) *
                    std::pow(-1, static_cast<int>(RLU() + 0.5));
          std::vector<double> LUPTDI_output = LUPTDI(0);
          PXO[0] = LUPTDI_output[0];
          PYO[0] = LUPTDI_output[1];
          WO[0] = WF;

          // Initial values for gluon treated like quark-antiquark jet pair,
          // sharing energy according to Altarelli-Parisi splitting function.
        } else {
          NSTR = 2;
          if (MSTJ[1] == 4) MSTJ[90] = 1;
          KFLO[0] = static_cast<int>(1. + (2. + PARJ[1]) * RLU()) *
                    std::pow(-1, static_cast<int>(RLU() + 0.5));
          KFLO[1] = -KFLO[0];
          std::vector<double> LUPTDI_output = LUPTDI(0);
          PXO[0] = LUPTDI_output[0];
          PYO[0] = LUPTDI_output[1];
          PXO[1] = -PXO[0];
          PYO[1] = -PYO[0];
          WO[0] = WF * std::pow(RLU(), 1. / 3.);
          WO[1] = WF - WO[0];
        }

        // Initial values for rank, flavour, pT and W + .
        for (int ISTR = 1; ISTR < NSTR + 1; ++ISTR) {  // 220
          bool repeat180 = false;
          do {  // 180
            repeat180 = false;
            I = N;
            MSTU[89] = MSTU91;
            int IRANK = 0;
            int KFL1 = KFLO[ISTR - 1];
            double PX1 = PXO[ISTR - 1];
            double PY1 = PYO[ISTR - 1];
            double W = WO[ISTR - 1];

            // New hadron. Generate flavour and hadron species.
            do {  // 190
              I++;
              if (I >= MSTU[3] - MSTU[31] - NJET - 5) {
                LUERRM(11, "(LUINDF:) no more memory left in LUJETS");
                if (MSTU[20] >= 1) return;
              }
              IRANK++;
              K[0][I - 1] = 1;
              K[2][I - 1] = IP1;
              K[3][I - 1] = 0;
              K[4][I - 1] = 0;
              int KFL2 = 0;
              bool break190 = false;
              bool repeat200 = false;
              do {  // 200
                repeat200 = false;
                std::vector<int> LUKFDI_output = LUKFDI(KFL1, 0);
                KFL2 = LUKFDI_output[0];
                K[1][I - 1] = LUKFDI_output[1];
                if (K[2][I - 1] == 0) {
                  break190 = true;
                  break;
                }
                if (MSTJ[11] >= 3 && IRANK == 1 && std::abs(KFL1) <= 10 && std::abs(KFL2) > 10) {
                  if (RLU() > PARJ[18]) {
                    repeat200 = true;
                    continue;
                  }
                }
              } while (repeat200);
              if (break190) {
                repeat180 = true;
                break;
              }

              // Find hadron mass. Generate four-momentum.
              P[4][I - 1] = ULMASS(K[1][I - 1]);
              std::vector<double> LUPTDI_output = LUPTDI(KFL1);
              double PX2 = LUPTDI_output[0];
              double PY2 = LUPTDI_output[1];
              P[0][I - 1] = PX1 + PX2;
              P[1][I - 1] = PY1 + PY2;
              double PR =
                  P[4][I - 1] * P[4][I - 1] + P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1];
              double Z = LUZDIS(KFL1, KFL2, PR);
              int MZSAV = 0;
              if (std::abs(KFL1) >= 4 && std::abs(KFL1) <= 8 && MSTU[89] < 8) {
                MZSAV = 1;
                MSTU[89]++;
                MSTU[89 + MSTU[89]] = I;
                PARU[89 + MSTU[89]] = Z;
              }
              P[2][I - 1] = 0.5 * (Z * W - PR / std::max(1e-4, Z * W));
              P[3][I - 1] = 0.5 * (Z * W + PR / std::max(1e-4, Z * W));
              if (MSTJ[2] >= 1 && IRANK == 1 && KFLH >= 4 && P[2][I - 1] <= 0.001) {
                if (W >= P[4][I - 1] + 0.5 * PARJ[31]) {
                  repeat180 = true;
                  break;
                }
                P[2][I - 1] = 0.0001;
                P[3][I - 1] = std::sqrt(PR);
                Z = P[3][I - 1] / W;
              }

              // Remaining flavour and momentum.
              KFL1 = -KFL2;
              PX1 = -PX2;
              PY1 = -PY2;
              W *= (1. - Z);
              for (int J = 0; J < 5; ++J) {
                V[J][I - 1] = 0.;
              }

              // Check if pL acceptable. Go back for new hadron if enough energy.
              if (MSTJ[2] >= 0 && P[2][I - 1] < 0.) {
                I--;
                if (MZSAV == 1) MSTU[89]--;
              }
            } while (W > PARJ[30]);  // 190
          } while (repeat180);
          N = I;
        }  // 220
        if (MSTJ[2] % 5 == 4 && N == NSAV1) WF += 0.1 * PARJ[31];
      } while (MSTJ[2] % 5 == 4 && N == NSAV1);  // 170

      // Rotate jet to new direction.
      double THE = ULANGL(P[2][IP1 - 1],
                          std::sqrt(P[0][IP1 - 1] * P[0][IP1 - 1] + P[1][IP1 - 1] * P[1][IP1 - 1]));
      double PHI = ULANGL(P[0][IP1 - 1], P[1][IP1 - 1]);
      MSTU[32] = 1;
      LUDBRB(NSAV1 + 1, N, THE, PHI, 0., 0., 0.);
      K[3][K[2][IP1 - 1] - 1] = NSAV1 + 1;
      K[4][K[2][IP1 - 1] - 1] = N;

      // End of jet generation loop. Skip conservation in some cases.
    }                                      // 230
    if (NJET == 1 || MSTJ[2] <= 0) break;  // breaks out of 150, goto 490
    if (MSTJ[2] % 5 != 0 && N - NSAV - NJET < 2) {
      repeat150 = true;
      continue;
    }

    // Subtract off produced hadron flavours, finished if zero.
    for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
      int KFA = std::abs(K[1][Ii]);
      int KFLA = (KFA / 1000) % 10;
      int KFLB = (KFA / 100) % 10;
      int KFLC = (KFA / 10) % 10;
      int sgn = (K[1][Ii] < 0) ? -1 : 1;
      if (KFLA == 0) {
        if (KFLB <= 3) NFL[KFLB - 1] -= sgn * std::pow(-1, KFLB);
        if (KFLC <= 3) NFL[KFLC - 1] += sgn * std::pow(-1, KFLB);
      } else {
        if (KFLA <= 3) NFL[KFLA - 1] -= sgn;
        if (KFLB <= 3) NFL[KFLB - 1] -= sgn;
        if (KFLC <= 3) NFL[KFLC - 1] -= sgn;
      }
    }
    int NREQ = (std::abs(NFL[0]) + std::abs(NFL[1]) + std::abs(NFL[2]) -
                std::abs(NFL[0] + NFL[1] + NFL[2])) /
                   2 +
               std::abs(NFL[0] + NFL[1] + NFL[2]) / 3;
    int NREM = 0;
    if (NREQ != 0) {  // skip condition 320

      // Take away flavour of low-momentum particles until enough freedom.
      bool repeat250 = false;
      do {  // 250
        repeat250 = false;
        int IREM = 0;
        double P2MIN = PECM * PECM;
        for (int Ii = NSAV + NJET + 1; Ii < N + 1; ++Ii) {
          int P2 = P[0][Ii - 1] * P[0][Ii - 1] + P[1][Ii - 1] * P[1][Ii - 1] +
                   P[2][Ii - 1] * P[2][Ii - 1];
          if (K[0][Ii - 1] == 1 && P2 < P2MIN) {
            IREM = Ii;
            P2MIN = P2;
          }
        }
        if (IREM == 0) {
          repeat150 = true;
          break;
        }
        K[0][IREM - 1] = 7;
        int KFA = std::abs(K[1][IREM - 1]);
        int KFLA = (KFA / 1000) % 10;
        int KFLB = (KFA / 100) % 10;
        int KFLC = (KFA / 10) % 10;
        if (KFLA >= 4 || KFLB >= 4) K[0][IREM - 1] = 8;
        if (K[0][IREM - 1] == 8) {
          repeat250 = true;
          continue;
        }
        int sgn = (K[1][IREM - 1] < 0) ? -1 : 1;
        if (KFLA == 0) {
          if (KFLB <= 3) NFL[KFLB - 1] += sgn * std::pow(-1, KFLB);
          if (KFLC <= 3) NFL[KFLC - 1] -= sgn * std::pow(-1, KFLB);
        } else {
          if (KFLA <= 3) NFL[KFLA - 1] += sgn;
          if (KFLB <= 3) NFL[KFLB - 1] += sgn;
          if (KFLC <= 3) NFL[KFLC - 1] += sgn;
        }
        NREM++;
        NREQ = (std::abs(NFL[0]) + std::abs(NFL[1]) + std::abs(NFL[2]) -
                std::abs(NFL[0] + NFL[1] + NFL[2])) /
                   2 +
               std::abs(NFL[0] + NFL[1] + NFL[2]) / 3;
        if (NREQ > NREM) {
          repeat250 = true;
          continue;
        }
      } while (repeat250);
      if (repeat150) continue;

      for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
        if (K[0][Ii] == 8) K[0][Ii] = 1;
      }

      // Find combination of existing and new flavours for hadron.
      bool repeat280 = false;
      do {  // 280
        repeat280 = false;
        int NFET = 2;
        if (NFL[0] + NFL[1] + NFL[2] != 0) NFET = 3;
        if (NREQ < NREM) NFET = 1;
        if (std::abs(NFL[0]) + std::abs(NFL[1]) + std::abs(NFL[2]) == 0) NFET = 0;

        for (int J = 0; J < NFET; ++J) {
          IFET[J] = 1 + (std::abs(NFL[0]) + std::abs(NFL[1]) + std::abs(NFL[2])) * RLU();
          KFLF[J] = (NFL[0] < 0) ? -1 : 1;
          if (IFET[J] > std::abs(NFL[0])) {
            int sgn = (NFL[1] < 0) ? -1 : 1;
            KFLF[J] = 2 * sgn;
          }
          if (IFET[J] > std::abs(NFL[0]) + std::abs(NFL[1])) {
            int sgn = (NFL[2] < 0) ? -1 : 1;
            KFLF[J] = 3 * sgn;
          }
        }
        if (NFET == 2 && (IFET[0] == IFET[1] || KFLF[0] * KFLF[1] > 0)) {
          repeat280 = true;
          continue;
        }
        if (NFET == 3 && (IFET[0] == IFET[1] || IFET[0] == IFET[2] || IFET[1] == IFET[2] ||
                          KFLF[0] * KFLF[1] < 0 || KFLF[0] * KFLF[2] < 0 ||
                          KFLF[0] * (NFL[0] + NFL[1] + NFL[2]) < 0)) {
          repeat280 = true;
          continue;
        }
        if (NFET == 0) {
          KFLF[0] = 1 + static_cast<int>((2. + PARJ[1]) * RLU());
          KFLF[1] = -KFLF[0];
        }
        if (NFET == 1) {
          int sgn = (-KFLF[0] < 0) ? -1 : 1;
          KFLF[1] = (1 + static_cast<int>((2. + PARJ[1]) * RLU())) * sgn;
        }
        if (NFET <= 2) KFLF[2] = 0;
        int KFLFC = 0;
        if (KFLF[2] != 0) {
          int sgn = (KFLF[0] < 0) ? -1 : 1;
          KFLFC = (1000 * std::max(std::abs(KFLF[0]), std::abs(KFLF[2])) +
                   100 * std::min(std::abs(KFLF[0]), std::abs(KFLF[2])) + 1) *
                  sgn;
          if (KFLF[0] == KFLF[2] || (1. + 3. * PARJ[3]) * RLU() > 1.) {
            sgn = (KFLFC < 0) ? -1 : 1;
            KFLFC += 2 * sgn;
          }
        } else {
          KFLFC = KFLF[0];
        }
        std::vector<int> LUKFDI_output = LUKFDI(KFLFC, KFLF[1]);
        int KF = LUKFDI_output[1];
        if (KF == 0) {
          repeat280 = true;
          continue;
        }
        for (int J = 0; J < std::max(2, NFET); ++J) {
          int sgn = (KFLF[J] < 0) ? -1 : 1;
          NFL[static_cast<int>(std::abs(KFLF[J])) - 1] -= sgn;
        }

        // Store hadron at random among free positions.
        int NPOS = std::min(1 + static_cast<int>(RLU() * NREM), NREM);
        for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
          if (K[0][Ii] == 7) NPOS--;
          if (K[0][Ii] == 1 || NPOS != 0) continue;
          K[0][Ii] = 1;
          K[1][Ii] = KF;
          P[4][Ii] = ULMASS(K[1][Ii]);
          P[3][Ii] = std::sqrt(P[0][Ii] * P[0][Ii] + P[1][Ii] * P[1][Ii] + P[2][Ii] * P[2][Ii] +
                               P[4][Ii] * P[4][Ii]);
        }
        NREM--;
        NREQ = (std::abs(NFL[0]) + std::abs(NFL[1]) + std::abs(NFL[2]) -
                std::abs(NFL[0] + NFL[1] + NFL[2])) /
                   2 +
               std::abs(NFL[0] + NFL[1] + NFL[2]) / 3;
        if (NREM > 0) {
          repeat280 = true;
          continue;
        }
      } while (repeat280);
    }  // 320
    if (repeat150) continue;

    // Compensate for missing momentum in global scheme (3 options).
    if (MSTJ[2] % 5 != 0 && MSTJ[2] % 5 != 4) {
      for (int J = 0; J < 3; ++J) {
        PSI[J] = 0.;
        for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
          PSI[J] += P[J][Ii];
        }
      }
      PSI[3] = PSI[0] * PSI[0] + PSI[1] * PSI[1] + PSI[2] * PSI[2];
      double PWS = 0.;
      for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
        if (MSTJ[2] % 5 == 1) PWS += P[3][Ii];
        if (MSTJ[2] % 5 == 2)
          PWS += std::sqrt(P[4][Ii] * P[4][Ii] +
                           std::pow(PSI[0] * P[0][Ii] + PSI[1] * P[1][Ii] + PSI[2] * P[2][Ii], 2) /
                               PSI[3]);
        if (MSTJ[2] % 5 == 3) PWS++;
      }
      for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
        double PW = 0.;
        if (MSTJ[2] % 5 == 1) PW = P[3][Ii];
        if (MSTJ[2] % 5 == 2)
          PW = std::sqrt(P[4][Ii] * P[4][Ii] +
                         std::pow(PSI[0] * P[0][Ii] + PSI[1] * P[1][Ii] + PSI[2] * P[2][Ii], 2) /
                             PSI[3]);
        if (MSTJ[2] % 5 == 3) PW = 1.;
        for (int J = 0; J < 3; ++J) {
          P[J][Ii] -= PSI[J] * PW / PWS;
        }
        P[3][Ii] = std::sqrt(P[0][Ii] * P[0][Ii] + P[1][Ii] * P[1][Ii] + P[2][Ii] * P[2][Ii] +
                             P[4][Ii] * P[4][Ii]);
      }

      // Compensate for missing momentum withing each jet separately.
    } else if (MSTJ[2] % 5 == 4) {
      for (int Ii = N; Ii < N + NJET; ++Ii) {
        K[0][Ii] = 0;
        for (int J = 0; J < 5; ++J) {
          P[J][Ii] = 0.;
        }
      }
      for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
        int IR1 = K[2][Ii];
        int IR2 = N + IR1 - NSAV;
        K[0][IR2 - 1]++;
        double PLS =
            (P[0][Ii] * P[0][IR1 - 1] + P[1][Ii] * P[1][IR1 - 1] + P[2][Ii] * P[2][IR1 - 1]) /
            (P[0][IR1 - 1] * P[0][IR1 - 1] + P[1][IR1 - 1] * P[1][IR1 - 1] +
             P[2][IR1 - 1] * P[2][IR1 - 1]);
        for (int J = 0; J < 3; ++J) {
          P[J][IR2 - 1] += P[J][Ii] - PLS * P[J][IR1 - 1];
        }
        P[3][IR2 - 1] += P[3][Ii];
        P[4][IR2 - 1] += PLS;
      }
      double PSS = 0.;
      for (int Ii = N; Ii < N + NJET; ++Ii) {
        if (K[0][Ii] != 0) PSS += P[3][Ii] / (PECM * (0.8 * P[4][Ii] + 0.2));
      }
      for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
        int IR1 = K[2][Ii];
        int IR2 = N + IR1 - NSAV;
        double PLS =
            (P[0][Ii] * P[0][IR1 - 1] + P[1][Ii] * P[1][IR1 - 1] + P[2][Ii] * P[2][IR1 - 1]) /
            (P[0][IR1 - 1] * P[0][IR1 - 1] + P[1][IR1 - 1] * P[1][IR1 - 1] +
             P[2][IR1 - 1] * P[2][IR1 - 1]);
        for (int J = 0; J < 3; ++J) {
          P[J][Ii] += (1. / (P[4][IR2 - 1] * PSS) - 1.) * PLS * P[J][IR1 - 1] -
                      P[J][IR2 - 1] / K[0][IR2 - 1];
        }
        P[3][Ii] = std::sqrt(P[0][Ii] * P[0][Ii] + P[1][Ii] * P[1][Ii] + P[2][Ii] * P[2][Ii] +
                             P[4][Ii] * P[4][Ii]);
      }
    }

    // Scale momenta for energy conservation.
    if (MSTJ[2] % 5 != 0) {
      double PMS = 0.;
      double PES = 0.;
      double PQS = 0.;
      for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
        PMS += P[4][Ii];
        PES += P[3][Ii];
        PQS += P[4][Ii] * P[4][Ii] / P[3][Ii];
      }
      if (PMS >= PECM) {
        repeat150 = true;
        continue;
      }
      int NECO = 0;
      do {
        NECO++;
        double PFAC = (PECM - PQS) / (PES - PQS);
        PES = 0.;
        PQS = 0.;
        for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
          for (int J = 0; J < 3; ++J) {
            P[J][Ii] *= PFAC;
          }
          P[3][Ii] = std::sqrt(P[0][Ii] * P[0][Ii] + P[1][Ii] * P[1][Ii] + P[2][Ii] * P[2][Ii] +
                               P[4][Ii] * P[4][Ii]);
          PES += P[3][Ii];
          PQS += P[4][Ii] * P[4][Ii] / P[3][Ii];
        }
      } while (NECO < 10 && std::abs(PECM - PES) > 2e-6 * PECM);
    }
  } while (repeat150);

  // 490
  // Origin of produced particles and parton daughter pointers.
  for (int Ii = NSAV + NJET; Ii < N; ++Ii) {
    if (MSTU[15] != 2) K[2][Ii] = NSAV + 1;
    if (MSTU[15] == 2) K[2][Ii] = K[2][K[2][Ii] - 1];
  }
  for (int Ii = NSAV; Ii < NSAV + NJET; ++Ii) {
    int I1 = K[2][Ii] - 1;
    K[0][I1] += 10;
    if (MSTU[15] != 2) {
      K[3][I1] = NSAV + 1;
      K[4][I1] = NSAV + 1;
    } else {
      K[3][I1] += 1 - NJET;
      K[4][I1] += 1 - NJET;
      if (K[4][I1] < K[3][I1]) {
        K[3][I1] = 0;
        K[4][I1] = 0;
      }
    }
  }

  // Document independent fragmentation system. Remove copy of jets.
  NSAV++;
  K[0][NSAV - 1] = 11;
  K[1][NSAV - 1] = 93;
  K[2][NSAV - 1] = IP;
  K[3][NSAV - 1] = NSAV + 1;
  K[4][NSAV - 1] = N - NJET + 1;
  for (int J = 0; J < 4; ++J) {
    P[J][NSAV - 1] = DPS[J];
    V[J][NSAV - 1] = V[J][IP - 1];
  }
  P[4][NSAV - 1] = std::sqrt(
      std::max(0., DPS[3] * DPS[3] - DPS[0] * DPS[0] - DPS[1] * DPS[1] - DPS[2] * DPS[2]));
  V[4][NSAV - 1] = 0.;
  for (int Ii = NSAV + NJET - 1; Ii < N; ++Ii) {
    for (int J = 0; J < 5; ++J) {
      K[J][Ii - NJET + 1] = K[J][Ii];
      P[J][Ii - NJET + 1] = P[J][Ii];
      V[J][Ii - NJET + 1] = V[J][Ii];
    }
  }
  N += 1 - NJET;
  for (int IZ = MSTU90; IZ < MSTU[89]; ++IZ) {
    MSTU[90 + IZ] += 1 - NJET;
  }

  // Boost back particle system. Set production vertices.
  if (NJET != 1) LUDBRB(NSAV + 1, N, 0., 0., DPS[0] / DPS[3], DPS[1] / DPS[3], DPS[2] / DPS[3]);
  for (int Ii = NSAV; Ii < N; ++Ii) {
    for (int J = 0; J < 4; ++J) {
      V[J][Ii] = V[J][IP - 1];
    }
  }
  return;
}

void sophia_interface::LUPREP(int IP) {
  // Purpose: to rearrange partons along strings, to allow small systems
  // to collapse into one or two particles and to check flavours.
  double DPS[5] = {0.};
  double DPC[5] = {0.};
  double UE[5] = {0.};

  // Rearrange parton shower product listing along strings: begin loop.
  int I1 = N;
  for (int MQGST = 1; MQGST < 3; ++MQGST) {
    for (int I = std::max(1, IP); I < N + 1; ++I) {
      if (K[0][I - 1] != 3) continue;
      int KC = LUCOMP(K[1][I - 1]);
      if (KC == 0) continue;
      int KQ = KCHG[1][KC - 1];
      if (KQ == 0 || (MQGST == 1 && KQ == 2)) continue;

      // Pick up loose string end.
      int KCS = 4;
      int sgn = (K[1][I - 1] < 0) ? -1 : 1;
      if (KQ * sgn < 0) KCS = 5;
      int IA = I;
      int NSTP = 0;

      bool break100 = false;
      do {  // 100
        NSTP++;
        if (NSTP > 4 * N) {
          LUERRM(14, "LUPREP: caught in infinite loop");
          return;
        }

        // Copy undecayed parton.
        if (K[0][IA - 1] == 3) {
          if (I1 >= MSTU[3] - MSTU[31] - 5) {
            LUERRM(11, "LUPREP: no more memory left in LUJETS");
            return;
          }
          I1++;
          K[0][I1 - 1] = 2;
          if (NSTP >= 2 && std::abs(K[1][IA - 1]) != 21) K[0][I1 - 1] = 1;
          K[1][I1 - 1] = K[1][IA - 1];
          K[2][I1 - 1] = IA;
          K[3][I1 - 1] = 0;
          K[4][I1 - 1] = 0;
          for (int J = 0; J < 5; ++J) {
            P[J][I1 - 1] = P[J][IA - 1];
            V[J][I1 - 1] = V[J][IA - 1];
          }
          K[0][IA - 1] += 10;
          if (K[0][I1 - 1] == 1) {
            break100 = true;
            break;
          }
        }

        // Go to next parton in colour space.
        int IB = IA;
        int MREV = 0;
        if ((K[KCS - 1][IB - 1] / (MSTU[4] * MSTU[4])) % 2 == 0 &&
            K[KCS - 1][IB - 1] % MSTU[4] != 0) {
          IA = K[KCS - 1][IB - 1] % MSTU[4];
          K[KCS - 1][IB - 1] += MSTU[4] * MSTU[4];
          MREV = 0;
        } else {
          if (K[KCS - 1][IB - 1] >= 2 * MSTU[4] * MSTU[4] ||
              (K[KCS - 1][IB - 1] / MSTU[4]) % MSTU[4] == 0)
            KCS = 9 - KCS;
          IA = (K[KCS - 1][IB - 1] / MSTU[4]) % MSTU[4];
          K[KCS - 1][IB - 1] += 2 * MSTU[4] * MSTU[4];
          MREV = 1;
        }
        if (IA <= 0 || IA > N) {
          LUERRM(12, "LUPREP: colour rearrangement failed");
          return;
        }
        if ((K[3][IA - 1] / MSTU[4]) % MSTU[4] == IB || (K[4][IA - 1] / MSTU[4]) % MSTU[4] == IB) {
          if (MREV == 1) KCS = 9 - KCS;
          if ((K[KCS - 1][IA - 1] / MSTU[4]) % MSTU[4] != IB) KCS = 9 - KCS;
          K[KCS - 1][IA - 1] += 2 * MSTU[4] * MSTU[4];
        } else {
          if (MREV == 0) KCS = 9 - KCS;
          if (K[KCS - 1][IA - 1] % MSTU[4] != IB) KCS = 9 - KCS;
          K[KCS - 1][IA - 1] += MSTU[4] * MSTU[4];
        }
      } while (IA != I);  // 100
      if (break100) continue;
      K[0][I1 - 1] = 1;
    }
  }
  N = I1;
  if (MSTJ[13] < 0) return;

  // Find lowest-mass colour singlet jet system, OK if above threshold.
  if (MSTJ[13] == 0) {
    LUPREP_checkFlavour(IP);
    return;
  }
  int NS = N;
  int NSAV = 0;
  double PECM = 0.;
  do {  // 140
    int NSIN = N - NS;
    double PDM = 1. + PARJ[31];
    int IC = 0;
    int IC1 = 0;
    int IC2 = 0;
    for (int I = std::max(1, IP); I < NS + 1; ++I) {  // 190
      if (K[0][I - 1] != 1 && K[0][I - 1] != 2) {
        // do nothing
      } else if (K[0][I - 1] == 2 && IC == 0) {
        NSIN++;
        IC = I;
        for (int J = 0; J < 4; ++J) {
          DPS[J] = P[J][I - 1];
        }
        MSTJ[92] = 1;
        DPS[4] = ULMASS(K[1][I - 1]);
      } else if (K[0][I - 1] == 2) {
        for (int J = 0; J < 4; ++J) {
          DPS[J] += P[J][I - 1];
        }
      } else if (IC != 0 && KCHG[1][LUCOMP(K[1][I - 1]) - 1] != 0) {
        for (int J = 0; J < 4; ++J) {
          DPS[J] += P[J][I - 1];
        }
        MSTJ[92] = 1;
        DPS[4] += ULMASS(K[1][I - 1]);
        double PD = std::sqrt(std::max(0., DPS[3] * DPS[3] - DPS[0] * DPS[0] - DPS[1] * DPS[1] -
                                               DPS[2] * DPS[2])) -
                    DPS[4];
        if (PD < PDM) {
          PDM = PD;
          for (int J = 0; J < 5; ++J) {
            DPC[J] = DPS[J];
          }
          IC1 = IC;
          IC2 = I;
        }
        IC = 0;
      } else {
        NSIN++;
      }
    }  // 190
    if (PDM >= PARJ[31]) {
      LUPREP_checkFlavour(IP);
      return;
    }

    // Fill small-mass system as cluster.
    NSAV = N;
    PECM = std::sqrt(
        std::max(0., DPC[3] * DPC[3] - DPC[0] * DPC[0] - DPC[1] * DPC[1] - DPC[2] * DPC[2]));
    K[0][N] = 11;
    K[1][N] = 91;
    K[2][N] = IC1;
    K[3][N] = N + 2;
    K[4][N] = N + 3;
    P[0][N] = DPC[0];
    P[1][N] = DPC[1];
    P[2][N] = DPC[2];
    P[3][N] = DPC[3];
    P[4][N] = PECM;

    // Form two particles from flavours of lowest-mass system, if feasible.
    K[0][N + 1] = 1;
    K[0][N + 2] = 1;
    if (MSTU[15] != 2) {
      K[2][N + 1] = N + 1;
      K[2][N + 2] = N + 1;
    } else {
      K[2][N + 1] = IC1;
      K[2][N + 2] = IC2;
    }
    K[3][N + 1] = 0;
    K[3][N + 2] = 0;
    K[4][N + 1] = 0;
    K[4][N + 2] = 0;
    int KFLN = 0;
    if (std::abs(K[1][IC1 - 1]) != 21) {
      int KC1 = LUCOMP(K[1][IC1 - 1]);
      int KC2 = LUCOMP(K[1][IC2 - 1]);
      if (KC1 == 0 || KC2 == 0) {
        LUPREP_checkFlavour(IP);
        return;
      }
      int sgn1 = (K[1][IC1 - 1] < 0) ? -1 : 1;
      int KQ1 = KCHG[1][KC1 - 1] * sgn1;
      int sgn2 = (K[1][IC2 - 1] < 0) ? -1 : 1;
      int KQ2 = KCHG[1][KC2 - 1] * sgn2;
      if (KQ1 + KQ2 != 0) {
        LUPREP_checkFlavour(IP);
        return;
      }
      do {
        std::vector<int> LUKFDI_output1 = LUKFDI(K[1][IC1 - 1], 0);
        KFLN = LUKFDI_output1[0];
        K[1][N + 1] = LUKFDI_output1[1];
        std::vector<int> LUKFDI_output2 = LUKFDI(K[1][IC2 - 1], -KFLN);
        K[1][N + 2] = LUKFDI_output2[1];
      } while (K[1][N + 1] == 0 || K[1][N + 2] == 0);
    } else {
      if (std::abs(K[1][IC2 - 1]) != 21) {
        LUPREP_checkFlavour(IP);
        return;
      }
      do {
        std::vector<int> LUKFDI_output1 = LUKFDI(1 + static_cast<int>((2. + PARJ[1]) * RLU()), 0);
        KFLN = LUKFDI_output1[0];
        std::vector<int> LUKFDI_output2 = LUKFDI(KFLN, 0);
        int KFLM = LUKFDI_output2[0];
        K[1][N + 1] = LUKFDI_output2[1];
        std::vector<int> LUKFDI_output3 = LUKFDI(-KFLN, -KFLM);
        K[1][N + 2] = LUKFDI_output3[1];
      } while (K[1][N + 1] == 0 || K[1][N + 2] == 0);
    }
    P[4][N + 1] = ULMASS(K[1][N + 1]);
    P[4][N + 2] = ULMASS(K[1][N + 2]);
    if (P[4][N + 1] + P[4][N + 2] + PARJ[63] >= PECM && NSIN == 1) {
      LUPREP_checkFlavour(IP);
      return;
    }

    bool skipTo300 = false;
    do {  // skip conditions 260
      if (P[4][N + 1] + P[4][N + 2] + PARJ[63] >= PECM) break;

      // Perform two-particle decay of jet system, if possible.
      if (PECM >= 0.02 * DPC[3]) {
        double PA =
            std::sqrt((PECM * PECM - (P[4][N + 1] + P[4][N + 2]) * (P[4][N + 1] + P[4][N + 2])) *
                      (PECM * PECM - (P[4][N + 1] - P[4][N + 2]) * (P[4][N + 1] - P[4][N + 2]))) /
            (2. * PECM);
        UE[2] = 2. * RLU() - 1.;
        double PHI = PARU[1] * RLU();
        UE[0] = std::sqrt(1. - UE[2] * UE[2]) * std::cos(PHI);
        UE[1] = std::sqrt(1. - UE[2] * UE[2]) * std::sin(PHI);
        for (int J = 0; J < 3; ++J) {
          P[J][N + 1] = PA * UE[J];
          P[J][N + 2] = -PA * UE[J];
        }
        P[3][N + 1] = std::sqrt(PA * PA + P[4][N + 1] * P[4][N + 1]);
        P[3][N + 2] = std::sqrt(PA * PA + P[4][N + 2] * P[4][N + 2]);
        MSTU[32] = 1;
        LUDBRB(N + 2, N + 3, 0., 0., DPC[0] / DPC[3], DPC[1] / DPC[3], DPC[2] / DPC[3]);
      } else {
        int NP = 0;
        for (int Ii = IC1; Ii < IC2 + 1; ++Ii) {
          if (K[0][Ii - 1] == 1 || K[0][Ii - 1] == 2) NP++;
        }
        double HA = P[3][IC1 - 1] * P[3][IC2 - 1] - P[0][IC1 - 1] * P[0][IC2 - 1] -
                    P[1][IC1 - 1] * P[1][IC2 - 1] - P[2][IC1 - 1] * P[2][IC2 - 1];
        if (NP >= 3 || HA <= 1.25 * P[4][IC1 - 1] * P[4][IC2 - 1]) break;
        double HD1 = 0.5 * (P[4][N + 1] * P[4][N + 1] - P[4][IC1 - 1] * P[4][IC1 - 1]);
        double HD2 = 0.5 * (P[4][N + 2] * P[4][N + 2] - P[4][IC2 - 1] * P[4][IC2 - 1]);
        double HR =
            std::sqrt(std::max(0., ((HA - HD1 - HD2) * (HA - HD1 - HD2) -
                                    (P[4][N + 1] * P[4][N + 2]) * (P[4][N + 1] * P[4][N + 2])) /
                                       (HA * HA - (P[4][IC1 - 1] * P[4][IC2 - 1]) *
                                                      (P[4][IC1 - 1] * P[4][IC2 - 1])))) -
            1.;
        double HC = P[4][IC1 - 1] * P[4][IC1 - 1] + 2. * HA + P[4][IC2 - 1] * P[4][IC2 - 1];
        double HK1 = ((P[4][IC2 - 1] * P[4][IC2 - 1] + HA) * HR + HD1 - HD2) / HC;
        double HK2 = ((P[4][IC1 - 1] * P[4][IC1 - 1] + HA) * HR + HD2 - HD1) / HC;
        for (int J = 0; J < 4; ++J) {
          P[J][N + 1] = (1. + HK1) * P[J][IC1 - 1] - HK2 * P[J][IC2 - 1];
          P[J][N + 2] = (1. + HK2) * P[J][IC2 - 1] - HK1 * P[J][IC1 - 1];
        }
      }

      for (int J = 0; J < 4; ++J) {
        V[J][N] = V[J][IC1 - 1];
        V[J][N + 1] = V[J][IC1 - 1];
        V[J][N + 2] = V[J][IC2 - 1];
      }
      V[4][N] = 0.;
      V[4][N + 1] = 0.;
      V[4][N + 2] = 0.;
      N += 3;
      skipTo300 = true;
      break;
    } while (false);  // end skip condition 260

    // 260
    // Else form one particle from the flavours available, if possible.
    if (skipTo300 == false) {
      do {  // loop 260
        K[4][N] = N + 2;
        if (std::abs(K[1][IC1 - 1]) > 100 && std::abs(K[1][IC2 - 1]) > 100) {
          LUPREP_checkFlavour(IP);
          return;
        } else if (std::abs(K[1][IC1 - 1]) != 21) {
          std::vector<int> LUKFDI_output = LUKFDI(K[1][IC1 - 1], K[1][IC2 - 1]);
          K[1][N + 1] = LUKFDI_output[1];
        } else {
          KFLN = 1 + static_cast<int>((2. + PARJ[1]) * RLU());
          std::vector<int> LUKFDI_output = LUKFDI(KFLN, -KFLN);
          K[1][N + 1] = LUKFDI_output[1];
        }
      } while (K[1][N + 1] == 0);
      P[4][N + 1] = ULMASS(K[1][N + 1]);

      // Find parton/particle which combines to largest extra mass.
      int IR = 0;
      double HA = 0.;
      double HSM = 0.;
      for (int MCOMB = 1; MCOMB < 4; ++MCOMB) {
        if (IR != 0) continue;
        for (int Ii = std::max(1, IP); Ii < N + 1; ++Ii) {
          if (K[0][Ii - 1] <= 0 || K[0][Ii - 1] > 10 ||
              (Ii >= IC1 && Ii <= IC2 && K[0][Ii - 1] >= 1 && K[0][Ii - 1] <= 2))
            continue;
          int KCI = 0;
          if (MCOMB == 1) KCI = LUCOMP(K[1][Ii - 1]);
          if (MCOMB == 1 && KCI == 0) continue;
          if (MCOMB == 1 && KCHG[1][KCI - 1] == 0 && Ii <= NS) continue;
          if (MCOMB == 2 && std::abs(K[1][Ii - 1]) > 10 && std::abs(K[1][Ii - 1]) <= 100) continue;
          double HCR = DPC[3] * P[3][Ii - 1] - DPC[0] * P[0][Ii - 1] - DPC[1] * P[1][Ii - 1] -
                       DPC[2] * P[2][Ii - 1];
          double HSR =
              2. * HCR + PECM * PECM - P[4][N + 1] * P[4][N + 1] - 2. * P[4][N + 1] * P[4][Ii - 1];
          if (HSR > HSM) {
            IR = Ii;
            HA = HCR;
            HSM = HSR;
          }
        }
      }

      // Shuffle energy and momentum to put new particle on mass shell.
      if (IR != 0) {
        double HB = PECM * PECM + HA;
        double HC = P[4][N + 1] * P[4][N + 1] + HA;
        double HD = P[4][IR - 1] * P[4][IR - 1] + HA;
        double ARGHK2 = ((HB + HC) * (HB + HC) - 4. * (HB + HD) * P[4][N + 1] * P[4][N + 1]) /
                        (HA * HA - (PECM * P[4][IR - 1]) * (PECM * P[4][IR - 1]));
        double HK2 = 0.5 * (HB * std::sqrt(std::max(0., ARGHK2)) - (HB + HC)) / (HB + HD);
        double HK1 = (0.5 * (P[4][N + 1] * P[4][N + 1] - PECM * PECM) + HD * HK2) / HB;
        for (int J = 0; J < 4; ++J) {
          P[J][N + 1] = (1. + HK1) * DPC[J] - HK2 * P[J][IR - 1];
          P[J][IR - 1] = (1. + HK2) * P[J][IR - 1] - HK1 * DPC[J];
          V[J][N] = V[J][IC1 - 1];
          V[J][N + 1] = V[J][IC1 - 1];
        }
        V[4][N] = 0.;
        V[4][N + 1] = 0.;
        N += 2;
      } else {
        LUERRM(3, "LUPREP: no match for collapsing cluster");
        return;
      }
    }
    skipTo300 = false;

    // Mark collapsed system and store daughter pointers. Iterate.
    for (int Ii = IC1; Ii < IC2 + 1; ++Ii) {
      if ((K[0][Ii - 1] == 1 || K[0][Ii - 1] == 2) && KCHG[1][LUCOMP(K[1][Ii - 1]) - 1] != 0) {
        K[0][Ii - 1] += 10;
        if (MSTU[15] != 2) {
          K[3][Ii - 1] = NSAV + 1;
          K[4][Ii - 1] = NSAV + 1;
        } else {
          K[3][Ii - 1] = NSAV + 2;
          K[4][Ii - 1] = N;
        }
      }
    }
  } while (N < MSTU[3] - MSTU[31] - 5);  // 140
  LUPREP_checkFlavour(IP);
  return;
}

void sophia_interface::LUPREP_checkFlavour(int IP) {
  // Serves as a local function of LUPREP. Solves goto->320 statement.
  // When this function is called, LUPREP returns.
  // Check flavours and invariant masses in parton systems.
  int NP = 0;
  int KFN = 0;
  int KQS = 0;
  double DPS[5] = {0., 0., 0., 0., 0.};
  for (int I = std::max(1, IP) - 1; I < N; ++I) {
    if (K[0][I] <= 0 || K[0][I] > 10) continue;
    int KC = LUCOMP(K[1][I]);
    if (KC == 0) continue;
    int sgn = (K[1][I] < 0) ? -1 : 1;
    int KQ = KCHG[1][KC - 1] * sgn;
    if (KQ == 0) continue;
    NP++;
    if (KQ != 2) {
      KFN++;
      KQS += KQ;
      MSTJ[92] = 1;
      DPS[4] += ULMASS(K[1][I]);
    }
    for (int J = 0; J < 4; ++J) {
      DPS[J] += P[J][I];
    }
    if (K[0][I] == 1) {
      if (NP != 1 && (KFN == 1 || KFN >= 3 || KQS != 0))
        LUERRM(2, "LUPREP: unphysical flavour combination");
      if (NP != 1 && DPS[3] * DPS[3] - DPS[0] * DPS[0] - DPS[1] * DPS[1] - DPS[2] * DPS[2] <
                         std::pow(0.9 * PARJ[31] + DPS[4], 2))
        LUERRM(3, "LUPREP: too small mass in jet system");
      NP = 0;
      KFN = 0;
      KQS = 0;
      for (int J = 0; J < 5; ++J) {
        DPS[J] = 0.;
      }
    }
  }
  return;
}

double sophia_interface::LUZDIS(int KFL1, int KFL2, double PR) {
  // Purpose: to generate the longitudinal splitting variable Z.
  double Z = 0.;

  // Check if heavy flavour fragmentation.
  int KFLA = std::abs(KFL1);
  int KFLB = std::abs(KFL2);
  int KFLH = KFLA;
  if (KFLA >= 10) KFLH = (KFLA / 1000) % 10;

  // Lund symmetric scaling function: determine parameters of shape.
  if (MSTJ[10] == 1 || (MSTJ[10] == 3 && KFLH <= 3) || MSTJ[10] >= 4) {
    double FA = PARJ[40];
    if (MSTJ[90] == 1) FA = PARJ[42];
    if (KFLB >= 10) FA += PARJ[44];
    double FBB = PARJ[41];
    if (MSTJ[90] == 1) FBB = PARJ[43];
    double FB = FBB * PR;
    double FC = 1.;
    if (KFLA >= 10) FC -= PARJ[44];
    if (KFLB >= 10) FC += PARJ[44];
    if (MSTJ[10] >= 4 && KFLH >= 4 && KFLH <= 5) {
      double FRED = PARJ[45];
      if (MSTJ[10] == 5 && KFLH == 5) FRED = PARJ[46];
      FC += FRED * FBB * PARF[100 + KFLH - 1] * PARF[100 + KFLH - 1];
    } else if (MSTJ[10] >= 4 && KFLH >= 6 && KFLH <= 8) {
      double FRED = PARJ[45];
      if (MSTJ[10] == 5) FRED = PARJ[47];
      FC += FRED * FBB * PMAS[0][KFLH - 1] * PMAS[0][KFLH - 1];
    }
    int MC = 1;
    if (std::abs(FC - 1.) > 0.01) MC = 2;

    // Determine position of maximum. Special cases for a  =  0 or a  =  c.
    double ZMAX = 0.;
    int MA = 0;
    if (FA < 0.02) {
      MA = 1;
      ZMAX = 1.;
      if (FC > FB) ZMAX = FB / FC;
    } else if (std::abs(FC - FA) < 0.01) {
      MA = 2;
      ZMAX = FB / (FB + FC);
    } else {
      MA = 3;
      ZMAX = 0.5 * (FB + FC - std::sqrt((FB - FC) * (FB - FC) + 4. * FA * FB)) / (FC - FA);
      if ((ZMAX > 0.9999) && (FB > 100.)) ZMAX = std::min(ZMAX, 1. - FA / FB);
    }

    // Subdivide z range if distribution very peaked near endpoint.
    int MMAX = 2;
    double ZDIV = 0.;
    double FINT = 0.;
    double ZDIVC = 0.;
    if (ZMAX < 0.1) {
      MMAX = 1;
      ZDIV = 2.75 * ZMAX;
      if (MC == 1) {
        FINT = 1. - std::log(ZDIV);
      } else {
        ZDIVC = std::pow(ZDIV, 1. - FC);
        FINT = 1. + (1. - 1. / ZDIVC) / (FC - 1.);
      }
    } else if (ZMAX > 0.85 && FB > 1.) {
      MMAX = 3;
      double FSCB = std::sqrt(4. + (FC / FB) * (FC / FB));
      ZDIV = FSCB - 1. / ZMAX - (FC / FB) * std::log(ZMAX * 0.5 * (FSCB + FC / FB));
      if (MA >= 2) ZDIV += (FA / FB) * std::log(1. - ZMAX);
      ZDIV = std::min(ZMAX, std::max(0., ZDIV));
      FINT = 1. + FB * (1. - ZDIV);
    }

    // Choice of z, preweighted for peaks at low or high z.
    bool repeat100 = false;
    do {  // 100
      repeat100 = false;
      Z = RLU();
      double FPRE = 1.;
      if (MMAX == 1) {
        if (FINT * RLU() <= 1.) {
          Z *= ZDIV;
        } else if (MC == 1) {
          Z = std::pow(ZDIV, Z);
          FPRE = ZDIV / Z;
        } else {
          Z = 1. / std::pow(ZDIVC + Z * (1. - ZDIVC), 1. / (1. - FC));
          FPRE = std::pow(ZDIV / Z, FC);
        }
      } else if (MMAX == 3) {
        if (FINT * RLU() <= 1.) {
          Z = ZDIV + std::log(Z) / FB;
          FPRE = std::exp(FB * (Z - ZDIV));
        } else {
          Z = ZDIV + Z * (1. - ZDIV);
        }
      }

      // Weighting according to correct formula.
      if (Z <= 0. || Z >= 1.) {
        repeat100 = true;
        continue;
      }
      double FEXP = FC * std::log(ZMAX / Z) + FB * (1. / ZMAX - 1. / Z);
      if (MA >= 2) FEXP += FA * std::log((1. - Z) / (1. - ZMAX));
      double FVAL = std::exp(std::max(-50., std::min(50., FEXP)));
      if (FVAL < RLU() * FPRE) {
        repeat100 = true;
        continue;
      }
    } while (repeat100);

    // Generate z according to Field-Feynman, SLAC, (1-z)^c OR z^c.
  } else {
    double FC = PARJ[50 + std::max(1, KFLH) - 1];
    if (MSTJ[90] == 1) FC = PARJ[58];
    bool repeat110 = false;
    do {  // 110
      repeat110 = false;
      Z = RLU();
      if (FC >= 0. && FC <= 1.) {
        if (FC > RLU()) Z = 1. - std::pow(Z, 1. / 3.);
      } else if (FC > -1 && FC < 0.) {
        if (-4. * FC * Z * (1. - Z) * (1. - Z) <
            RLU() * std::pow((1. - Z) * (1. - Z) - FC * Z, 2)) {
          repeat110 = true;
          continue;
        }
      } else {
        if (FC > 0.) Z = 1. - std::pow(Z, (1. / FC));
        if (FC < 0.) Z = std::pow(Z, (-1. / FC));
      }
    } while (repeat110);
  }
  return Z;
}

std::vector<double> sophia_interface::LUPTDI(int KFL) {
  // Purpose: to generate transverse momentum according to a Gaussian.
  // output: vector = (PX, PY)
  // Generate p_T and azimuthal angle, gives p_x and p_y.
  int KFLA = std::abs(KFL);
  double PT = PARJ[20] * std::sqrt(-std::log(std::max(1e-10, RLU())));
  if (PARJ[22] > RLU()) PT *= PARJ[23];
  if (MSTJ[90] == 1) PT *= PARJ[21];
  if (KFLA == 0 && MSTJ[12] <= 0) PT = 0.;
  double PHI = PARU[1] * RLU();
  double PX = PT * std::cos(PHI);
  double PY = PT * std::sin(PHI);

  std::vector<double> output;
  output.push_back(PX);
  output.push_back(PY);
  return output;
}

std::vector<int> sophia_interface::LUKFDI(int KFL1, int KFL2) {
  // Purpose: to generate a new flavour pair and combine off a hadron.
  // Default flavour values. Input consistency checks.
  // Returns vector (KFL3, KF).
  int KFL3 = 0;
  int KF = 0;
  std::vector<int> output;
  int KF1A = std::abs(KFL1);
  int KF2A = std::abs(KFL2);
  if (KF1A == 0) {
    output.push_back(KFL3);
    output.push_back(KF);
    return output;
  }
  if (KF2A != 0) {
    if (KF1A <= 10 && KF2A <= 10 && KFL1 * KFL2 > 0) {
      output.push_back(KFL3);
      output.push_back(KF);
      return output;
    }
    if (KF1A > 10 && KF2A > 10) {
      output.push_back(KFL3);
      output.push_back(KF);
      return output;
    }
    if ((KF1A > 10 || KF2A > 10) && KFL1 * KFL2 < 0) {
      output.push_back(KFL3);
      output.push_back(KF);
      return output;
    }
  }

  // Check if tabulated flavour probabilities are to be used.
  bool skip = false;
  int KTAB1 = 0;
  int KTAB2 = 0;
  int KFL1A = 0;
  int KFL1B = 0;
  if (MSTJ[14] == 1) {
    KTAB1 = -1;
    if ((KF1A >= 1) && (KF1A <= 6)) KTAB1 = KF1A;
    KFL1A = (KF1A / 1000) % 10;
    KFL1B = (KF1A / 100) % 10;
    int KFL1S = KF1A % 10;
    if (KFL1A >= 1 && KFL1A <= 4 && KFL1B >= 1 && KFL1B <= 4)
      KTAB1 = 6 + KFL1A * (KFL1A - 2) + 2 * KFL1B + (KFL1S - 1) / 2;
    if (KFL1A >= 1 && KFL1A <= 4 && KFL1A == KFL1B) KTAB1--;
    if (KF1A >= 1 && KF1A <= 6) KFL1A = KF1A;
    KTAB2 = 0;
    if (KF2A != 0) {
      KTAB2 = -1;
      if (KF2A >= 1 && KF2A <= 6) KTAB2 = KF2A;
      int KFL2A = (KF2A / 1000) % 10;
      int KFL2B = (KF2A / 100) % 10;
      int KFL2S = KF2A % 10;
      if (KFL2A >= 1 && KFL2A <= 4 && KFL2B >= 1 && KFL2B <= 4)
        KTAB2 = 6 + KFL2A * (KFL2A - 2) + 2 * KFL2B + (KFL2S - 1) / 2;
      if (KFL2A >= 1 && KFL2A <= 4 && KFL2A == KFL2B) KTAB2--;
    }
    if (KTAB1 >= 0 && KTAB2 >= 0) skip = true;
  }

  // Parameters and breaking diquark parameter combinations.
  int KFL3A = 0;
  int KFLA = 0;
  int KFLB = 0;
  int KFLC = 0;
  int KFS = 0;
  bool repeat100 = false;
  do {
    repeat100 = false;
    if (skip == false) {
      double PAR2 = PARJ[1];
      double PAR3 = PARJ[2];
      double PAR4 = 3. * PARJ[3];
      double PAR3M = 0.;
      double PAR4M = 0.;
      double PARDM = 0.;
      double PARS0 = 0.;
      double PARS1 = 0.;
      double PARS2 = 0.;
      double PARSM = 0.;
      if (MSTJ[11] >= 2) {
        PAR3M = std::sqrt(PARJ[2]);
        PAR4M = 1. / (3. * std::sqrt(PARJ[3]));
        PARDM = PARJ[6] / (PARJ[6] + PAR3M * PARJ[5]);
        PARS0 = PARJ[4] * (2. + (1. + PAR2 * PAR3M * PARJ[6]) * (1. + PAR4M));
        PARS1 = PARJ[6] * PARS0 / (2. * PAR3M) +
                PARJ[4] * (PARJ[5] * (1. + PAR4M) + PAR2 * PAR3M * PARJ[5] * PARJ[6]);
        PARS2 = PARJ[4] * 2. * PARJ[5] * PARJ[6] * (PAR2 * PARJ[6] + (1. + PAR4M) / PAR3M);
        PARSM = std::max({PARS0, PARS1, PARS2});
        PAR4 = PAR4 * (1. + PARSM) / (1. + PARSM / (3. * PAR4M));
      }

      // Choice of whether to generate meson or baryon.
      bool repeat110 = false;
      do {
        repeat110 = false;
        int MBARY = 0;
        int KFDA = 0;
        if (KF1A <= 10) {
          if (KF2A == 0 && MSTJ[11] >= 1 && ((1. + PARJ[0]) * RLU()) > 1.) MBARY = 1;
          if (KF2A > 10) MBARY = 2;
          if (KF2A > 10 && KF2A <= 10000) KFDA = KF2A;
        } else {
          MBARY = 2;
          if (KF1A <= 10000) KFDA = KF1A;
        }

        // Possibility of process diquark -> meson + new diquark.
        int KFLDS = 0;
        if (KFDA != 0 && MSTJ[11] >= 2) {
          int KFLDA = (KFDA / 1000) % 10;
          int KFLDB = (KFDA / 100) % 10;
          KFLDS = KFDA % 10;
          double WTDQ = PARS0;
          if (std::max(KFLDA, KFLDB) == 3) WTDQ = PARS1;
          if (std::min(KFLDA, KFLDB) == 3) WTDQ = PARS2;
          if (KFLDS == 1) WTDQ /= (3. * PAR4M);
          if ((1. + WTDQ) * RLU() > 1.) MBARY = -1;
          if (MBARY == -1 && KF2A != 0) {
            output.push_back(KFL3);
            output.push_back(KF);
            return output;
          }
        }

        // Flavour for meson, possibly with new flavour.
        int KFLS = 0;
        if (MBARY <= 0) {
          KFS = (KFL1 < 0) ? -1 : 1;
          if (MBARY == 0) {
            int sgn = (-KFL1 < 0) ? -1 : 1;
            if (KF2A == 0) KFL3 = (1 + static_cast<int>((2. + PAR2) * RLU())) * sgn;
            KFLA = std::max(KF1A, KF2A + std::abs(KFL3));
            KFLB = std::min(KF1A, KF2A + std::abs(KFL3));
            if (KFLA != KF1A) KFS = -KFS;

            // Splitting of diquark into meson plus new diquark.
          } else {
            int KFL1A = (KF1A / 1000) % 10;
            int KFL1B = (KF1A / 100) % 10;
            int KFL1D = 0;
            int KFL1E = 0;
            do {
              KFL1D = KFL1A + static_cast<int>(RLU() + 0.5) * (KFL1B - KFL1A);
              KFL1E = KFL1A + KFL1B - KFL1D;
              if ((KFL1D == 3 && RLU() > PARDM) || (KFL1E == 3 && RLU() < PARDM)) {
                KFL1D = KFL1A + KFL1B - KFL1D;
                KFL1E = KFL1A + KFL1B - KFL1E;
              }
              KFL3A = 1 + static_cast<int>((2. + PAR2 * PAR3M * PARJ[6]) * RLU());
            } while ((KFL1E != KFL3A && (RLU() > (1. + PAR4M) / std::max(2., 1. + PAR4M))) ||
                     (KFL1E == KFL3A && (RLU() > 2. / std::max(2., 1. + PAR4M))));
            KFLDS = 3;
            if (KFL1E != KFL3A) KFLDS = 2 * static_cast<int>(RLU() + 1. / (1. + PAR4M)) + 1;
            int sgn = (-KFL1 < 0) ? -1 : 1;
            KFL3 = (10000 + 1000 * std::max(KFL1E, KFL3A) + 100 * std::min(KFL1E, KFL3A) + KFLDS) *
                   sgn;
            KFLA = std::max(KFL1D, KFL3A);
            KFLB = std::min(KFL1D, KFL3A);
            if (KFLA != KFL1D) KFS = -KFS;
          }

          // Form meson, with spin and flavour mixing for diagonal states.
          int KMUL = 0;
          if (KFLA <= 2) KMUL = static_cast<int>(PARJ[10] + RLU());
          if (KFLA == 3) KMUL = static_cast<int>(PARJ[11] + RLU());
          if (KFLA >= 4) KMUL = static_cast<int>(PARJ[12] + RLU());
          if (KMUL == 0 && PARJ[13] > 0.) {
            if (RLU() < PARJ[13]) KMUL = 2;
          } else if (KMUL == 1 && PARJ[14] + PARJ[15] + PARJ[16] > 0.) {
            double RMUL = RLU();
            if (RMUL < PARJ[14]) KMUL = 3;
            if (KMUL == 1 && RMUL < PARJ[14] + PARJ[15]) KMUL = 4;
            if (KMUL == 1 && RMUL < PARJ[14] + PARJ[15] + PARJ[16]) KMUL = 5;
          }
          KFLS = 3;
          if (KMUL == 0 || KMUL == 3) KFLS = 1;
          if (KMUL == 5) KFLS = 5;
          if (KFLA != KFLB) {
            KF = (100 * KFLA + 10 * KFLB + KFLS) * KFS * std::pow(-1, KFLA);
          } else {
            double RMIX = RLU();
            int IMIX = 2 * KFLA + 10 * KMUL;
            if (KFLA <= 3)
              KF = 110 * (1 + static_cast<int>(RMIX + PARF[IMIX - 2]) +
                          static_cast<int>(RMIX + PARF[IMIX - 1])) +
                   KFLS;
            if (KFLA >= 4) KF = 110 * KFLA + KFLS;
          }
          if (KMUL == 2 || KMUL == 3) {
            int sgn = (KF < 0) ? -1 : 1;
            KF += 10000 * sgn;
          }
          if (KMUL == 4) {
            int sgn = (KF < 0) ? -1 : 1;
            KF += 20000 * sgn;
          }

          // Optional extra suppression of eta and eta'.
          if (KF == 221) {
            if (RLU() > PARJ[24]) {
              repeat110 = true;
              continue;
            }
          } else if (KF == 331) {
            if (RLU() > PARJ[25]) {
              repeat110 = true;
              continue;
            }
          }

          // Generate diquark flavour.
        } else {
          double WT = 0.;
          int KBARY = 0.;
          do {
            if (KF1A <= 10 && KF2A == 0) {
              KFLA = KF1A;
              do {
                KFLB = 1 + static_cast<int>((2. + PAR2 * PAR3) * RLU());
                KFLC = 1 + static_cast<int>((2. + PAR2 * PAR3) * RLU());
                KFLDS = 1;
                if (KFLB >= KFLC) KFLDS = 3;
              } while ((KFLDS == 1 && PAR4 * RLU() > 1.) || (KFLDS == 3 && PAR4 < RLU()));
              int sgn = (KFL1 < 0) ? -1 : 1;
              KFL3 = (1000 * std::max(KFLB, KFLC) + 100 * std::min(KFLB, KFLC) + KFLDS) * sgn;

              // Take diquark flavour from input.
            } else if (KF1A <= 10) {
              KFLA = KF1A;
              KFLB = (KF2A / 1000) % 10;
              KFLC = (KF2A / 100) % 10;
              KFLDS = KF2A % 10;

              // Generate (or take from input) quark to go with diquark.
            } else {
              if (KF2A == 0) {
                int sgn = (KFL1 < 0) ? -1 : 1;
                KFL3 = (1 + static_cast<int>((2. + PAR2) * RLU())) * sgn;
              }
              KFLA = KF2A + std::abs(KFL3);
              KFLB = (KF1A / 1000) % 10;
              KFLC = (KF1A / 100) % 10;
              KFLDS = KF1A % 10;
            }

            // SU(6) factors for formation of baryon. Try again if fails.
            KBARY = KFLDS;
            if (KFLDS == 3 && KFLB != KFLC) KBARY = 5;
            if (KFLA != KFLB && KFLA != KFLC) KBARY++;
            WT = PARF[59 + KBARY] + PARJ[17] * PARF[69 + KBARY];
            if (MBARY == 1 && MSTJ[11] >= 2) {
              double WTDQ = PARS0;
              if (std::max(KFLB, KFLC) == 3) WTDQ = PARS1;
              if (std::min(KFLB, KFLC) == 3) WTDQ = PARS2;
              if (KFLDS == 1) WTDQ /= (3. * PAR4M);
              if (KFLDS == 1) WT = WT * (1. + WTDQ) / (1. + PARSM / (3. * PAR4M));
              if (KFLDS == 3) WT = WT * (1. + WTDQ) / (1. + PARSM);
            }
          } while (KF2A == 0 && WT < RLU());

          // Form baryon. Distinguish Lambda- and Sigmalike baryons.
          int KFLD = std::max({KFLA, KFLB, KFLC});
          int KFLF = std::min({KFLA, KFLB, KFLC});
          int KFLE = KFLA + KFLB + KFLC - KFLD - KFLF;
          KFLS = 2;
          if ((PARF[59 + KBARY] + PARJ[17] * PARF[69 + KBARY]) * RLU() > PARF[59 + KBARY]) KFLS = 4;
          int KFLL = 0;
          if (KFLS == 2 && KFLD > KFLE && KFLE > KFLF) {
            if (KFLDS == 1 && KFLA == KFLD) KFLL = 1;
            if (KFLDS == 1 && KFLA != KFLD) KFLL = static_cast<int>(0.25 + RLU());
            if (KFLDS == 3 && KFLA != KFLD) KFLL = static_cast<int>(0.75 + RLU());
          }
          if (KFLL == 0) {
            int sgn = (KFL1 < 0) ? -1 : 1;
            KF = (1000 * KFLD + 100 * KFLE + 10 * KFLF + KFLS) * sgn;
          }
          if (KFLL == 1) {
            int sgn = (KFL1 < 0) ? -1 : 1;
            KF = (1000 * KFLD + 100 * KFLF + 10 * KFLE + KFLS) * sgn;
          }
        }

        output.push_back(KFL3);
        output.push_back(KF);
        return output;
      } while (repeat110);
    }  // end skip condition
    skip = false;

    // Use tabulated probabilities to select new flavour and hadron.
    int KT3L = KTAB2;
    int KT3U = KTAB2;
    if (KTAB2 == 0 && MSTJ[11] <= 0) {
      KT3L = 1;
      KT3U = 6;
    } else if (KTAB2 == 0 && KTAB1 >= 7 && MSTJ[11] <= 1) {
      KT3L = 1;
      KT3U = 6;
    } else if (KTAB2 == 0) {
      KT3L = 1;
      KT3U = 22;
    }
    double RFL = 0.;
    for (int KTS = 0; KTS < 3; ++KTS) {
      for (int KT3 = KT3L; KT3 < KT3U + 1; ++KT3) {
        RFL += PARF[120 + 80 * KTAB1 + 25 * KTS + KT3 - 1];
      }
    }
    RFL *= RLU();
    int KTABS = 0;
    int KTAB3 = 0;
    for (int KTS = 0; KTS < 3; ++KTS) {
      KTABS = KTS;
      bool breakLoop = false;
      for (int KT3 = KT3L; KT3 < KT3U + 1; ++KT3) {
        KTAB3 = KT3;
        RFL -= PARF[120 + 80 * KTAB1 + 25 * KTS + KT3 - 1];
        if (RFL <= 0.) {
          breakLoop = true;
          break;
        }
      }
      if (breakLoop) break;
    }

    // Reconstruct flavour of produced quark/diquark.
    int KFL3B = 0;
    if (KTAB3 <= 6) {
      KFL3A = KTAB3;
      KFL3B = 0;
      int sgn = (KFL1 * (2 * KTAB1 - 13) < 0) ? -1 : 1;
      KFL3 = KFL3A * sgn;
    } else {
      KFL3A = 1;
      if (KTAB3 >= 8) KFL3A = 2;
      if (KTAB3 >= 11) KFL3A = 3;
      if (KTAB3 >= 16) KFL3A = 4;
      KFL3B = (KTAB3 - 6 - KFL3A * (KFL3A - 2)) / 2;
      KFL3 = 1000 * KFL3A + 100 * KFL3B + 1;
      if (KFL3A == KFL3B || KTAB3 != 6 + KFL3A * (KFL3A - 2) + 2 * KFL3B) KFL3 += 2;
      int sgn = (KFL1 * (13 - 2 * KTAB1) < 0) ? -1 : 1;
      KFL3 = KFL3 * sgn;
    }

    // Reconstruct meson code.
    if (KFL3A == KFL1A && KFL3B == KFL1B && (KFL3A <= 3 || KFL3B != 0)) {
      RFL = RLU() *
            (PARF[143 + 80 * KTAB1 + 25 * KTABS - 1] + PARF[144 + 80 * KTAB1 + 25 * KTABS - 1] +
             PARF[145 + 80 * KTAB1 + 25 * KTABS - 1]);
      KF = 110 + 2 * KTABS + 1;
      if (RFL > PARF[143 + 80 * KTAB1 + 25 * KTABS - 1]) KF = 220 + 2 * KTABS + 1;
      if (RFL > PARF[143 + 80 * KTAB1 + 25 * KTABS - 1] + PARF[144 + 80 * KTAB1 + 25 * KTABS - 1])
        KF = 330 + 2 * KTABS + 1;
    } else if (KTAB1 <= 6 && KTAB3 <= 6) {
      KFLA = std::max(KTAB1, KTAB3);
      KFLB = std::min(KTAB1, KTAB3);
      KFS = (KFL1 < 0) ? -1 : 1;
      if (KFLA != KF1A) KFS = -KFS;
      KF = (100 * KFLA + 10 * KFLB + 2 * KTABS + 1) * KFS * std::pow(-1, KFLA);
    } else if (KTAB1 >= 7 && KTAB3 >= 7) {
      KFS = (KFL1 < 0) ? -1 : 1;
      if (KFL1A == KFL3A) {
        KFLA = std::max(KFL1B, KFL3B);
        KFLB = std::min(KFL1B, KFL3B);
        if (KFLA != KFL1B) KFS = -KFS;
      } else if (KFL1A == KFL3B) {
        KFLA = KFL3A;
        KFLB = KFL1B;
        KFS = -KFS;
      } else if (KFL1B == KFL3A) {
        KFLA = KFL1A;
        KFLB = KFL3B;
      } else if (KFL1B == KFL3B) {
        KFLA = std::max(KFL1A, KFL3A);
        KFLB = std::min(KFL1A, KFL3A);
        if (KFLA != KFL1A) KFS = -KFS;
      } else {
        LUERRM(2, "LUKFDI: no matching flavours for qq -> qq");
        repeat100 = true;
        continue;
      }
      KF = (100 * KFLA + 10 * KFLB + 2 * KTABS + 1) * KFS * std::pow(-1, KFLA);

      // Reconstruct baryon code.
    } else {
      if (KTAB1 >= 7) {
        KFLA = KFL3A;
        KFLB = KFL1A;
        KFLC = KFL1B;
      } else {
        KFLA = KFL1A;
        KFLB = KFL3A;
        KFLC = KFL3B;
      }
      int KFLD = std::max({KFLA, KFLB, KFLC});
      int KFLF = std::min({KFLA, KFLB, KFLC});
      int KFLE = KFLA + KFLB + KFLC - KFLD - KFLF;
      if (KTABS == 0) {
        int sgn = (KFL1 < 0) ? -1 : 1;
        KF = (1000 * KFLD + 100 * KFLF + 10 * KFLE + 2) * sgn;
      }
      if (KTABS >= 1) {
        int sgn = (KFL1 < 0) ? -1 : 1;
        KF = (1000 * KFLD + 100 * KFLE + 10 * KFLF + 2 * KTABS) * sgn;
      }
    }

    // Check that constructed flavour code is an allowed one.
    if (KFL2 != 0) KFL3 = 0;
    int KC = LUCOMP(KF);
    if (KC == 0) {
      LUERRM(2, "LUKFDI: user-defined flavour probabilities failed");
      repeat100 = true;
      continue;
    }
  } while (repeat100);

  output.push_back(KFL3);
  output.push_back(KF);
  return output;
}

void sophia_interface::LUEDIT(int MEDIT) {
  // Purpose: to perform global manipulations on the event record,
  // in particular to exclude unstable or undetectable partons/particles.
  int NS[2];
  double PTS[2];
  double PLS[2];

  // Remove unwanted partons/particles.
  if ((MEDIT >= 0 && MEDIT <= 3) || MEDIT == 5) {
    int IMAX = N;
    if (MSTU[1] > 0) IMAX = MSTU[1];
    int I1 = std::max(1, MSTU[0]) - 1;
    for (int I = std::max(1, MSTU[0]) - 1; I < IMAX; ++I) {
      if (K[0][I] == 0 || K[0][I] > 20) continue;
      if (MEDIT == 1) {
        if (K[0][I] > 10) continue;
      } else if (MEDIT == 2) {
        if (K[0][I] > 10) continue;
        int KC = LUCOMP(K[1][I]);
        if (KC == 0 || KC == 12 || KC == 14 || KC == 16 || KC == 18) continue;
      } else if (MEDIT == 3) {
        if (K[0][I] > 10) continue;
        int KC = LUCOMP(K[1][I]);
        if (KC == 0) continue;
        if (KCHG[1][KC - 1] == 0 && LUCHGE(K[1][I]) == 0) continue;
      } else if (MEDIT == 5) {
        if (K[0][I] == 13 || K[0][I] == 14) continue;
        int KC = LUCOMP(K[1][I]);
        if (KC == 0) continue;
        if (K[0][I] >= 11 && KCHG[1][KC - 1] == 0) continue;
      }

      // Pack remaining partons/particles. Origin no longer known.
      I1++;
      for (int J = 0; J < 5; ++J) {
        K[J][I1 - 1] = K[J][I];
        P[J][I1 - 1] = P[J][I];
        V[J][I1 - 1] = V[J][I];
      }
      K[2][I1 - 1] = 0;
    }
    if (I1 < N) MSTU[2] = 0;
    if (I1 < N) MSTU[69] = 0;
    N = I1;

    // Selective removal of class of entries. New position of retained.
  } else if (MEDIT >= 11 && MEDIT <= 15) {
    int I1 = 0;
    for (int I = 0; I < N; ++I) {
      K[2][I] = K[2][I] % MSTU[4];
      if (MEDIT == 11 && K[0][I] < 0) continue;
      if (MEDIT == 12 && K[0][I] == 0) continue;
      if (MEDIT == 13 && (K[0][I] == 11 || K[0][I] == 12 || K[0][I] == 15) && K[1][I] != 94)
        continue;
      if (MEDIT == 14 && (K[0][I] == 13 || K[0][I] == 14 || K[1][I] == 94)) continue;
      if (MEDIT == 15 && K[0][I] >= 21) continue;
      I1++;
      K[2][I] = K[2][I] + MSTU[4] * I1;
    }

    // Find new event history information and replace old.
    for (int I = 1; I < N + 1; ++I) {
      if (K[0][I - 1] <= 0 || K[0][I - 1] > 20 || K[2][I - 1] / MSTU[4] == 0) continue;
      int ID = I;
      int IM = 0;
      bool repeat130 = false;
      do {
        repeat130 = false;
        IM = K[2][ID - 1] % MSTU[4];
        if (MEDIT == 13 && IM > 0 && IM <= N) {
          if ((K[0][IM - 1] == 11 || K[0][IM - 1] == 12 || K[0][IM - 1] == 15) &&
              K[1][IM - 1] != 94) {
            ID = IM;
            repeat130 = true;
            continue;
          }
        } else if (MEDIT == 14 && IM > 0 && IM <= N) {
          if (K[0][IM - 1] == 13 || K[0][IM - 1] == 14 || K[1][IM - 1] == 94) {
            ID = IM;
            repeat130 = true;
            continue;
          }
        }
      } while (repeat130);

      K[2][I - 1] = MSTU[4] * (K[2][I - 1] / MSTU[4]);
      if (IM != 0) K[2][I - 1] += K[2][IM - 1] / MSTU[4];
      if (K[0][I - 1] != 3 && K[0][I - 1] != 13 && K[0][I - 1] != 14) {
        if (K[3][I - 1] > 0 && K[3][I - 1] <= MSTU[3])
          K[3][I - 1] = K[2][K[3][I - 1] - 1] / MSTU[4];
        if (K[4][I - 1] > 0 && K[4][I - 1] <= MSTU[3])
          K[4][I - 1] = K[2][K[4][I - 1] - 1] / MSTU[4];
      } else {
        int KCM = (K[3][I - 1] / MSTU[4]) % MSTU[4];
        if (KCM > 0 && KCM <= MSTU[3]) KCM = K[2][KCM - 1] / MSTU[4];
        int KCD = K[3][I - 1] % MSTU[4];
        if (KCD > 0 && KCD <= MSTU[3]) KCD = K[2][KCD - 1] / MSTU[4];
        K[3][I - 1] = MSTU[4] * MSTU[4] * (K[3][I - 1] / (MSTU[4] * MSTU[4])) + MSTU[4] * KCM + KCD;
        KCM = (K[4][I - 1] / MSTU[4]) % MSTU[4];
        if (KCM > 0 && KCM <= MSTU[3]) KCM = K[2][KCM - 1] / MSTU[4];
        KCD = K[4][I - 1] % MSTU[4];
        if (KCD > 0 && KCD <= MSTU[3]) KCD = K[2][KCD - 1] / MSTU[4];
        K[4][I - 1] = MSTU[4] * MSTU[4] * (K[4][I - 1] / (MSTU[4] * MSTU[4])) + MSTU[4] * KCM + KCD;
      }
    }

    // Pack remaining entries.
    I1 = 0;
    int MSTU90 = MSTU[89];
    MSTU[89] = 0;
    for (int I = 0; I < N; ++I) {
      if (K[2][I] / MSTU[4] == 0) continue;
      I1++;
      for (int J = 0; J < 5; ++J) {
        K[J][I1 - 1] = K[J][I];
        P[J][I1 - 1] = P[J][I];
        V[J][I1 - 1] = V[J][I];
      }
      K[2][I1 - 1] = K[2][I1 - 1] % MSTU[4];
      for (int IZ = 0; IZ < MSTU90; ++IZ) {
        if (I + 1 == MSTU[90 + IZ]) {
          MSTU[89]++;
          MSTU[89 + MSTU[89]] = I1;
          PARU[89 + MSTU[89]] = PARU[90 + IZ];
        }
      }
    }
    if (I1 < N) MSTU[2] = 0;
    if (I1 < N) MSTU[69] = 0;
    N = I1;

    // Fill in some missing daughter pointers (lost in colour flow).
  } else if (MEDIT == 16) {
    for (int I = 1; I < N + 1; ++I) {
      if (K[0][I - 1] <= 10 || K[0][I - 1] > 20) continue;
      if (K[3][I - 1] != 0 || K[4][I - 1] != 0) continue;

      // Find daughters who point to mother.
      for (int I1 = I + 1; I1 < N + 1; ++I1) {
        if (K[2][I1 - 1] != I) {
          // do nothing
        } else if (K[3][I - 1] == 0) {
          K[3][I - 1] = I1;
        } else {
          K[4][I - 1] = I1;
        }
      }
      if (K[4][I - 1] == 0) K[4][I - 1] = K[3][I - 1];
      if (K[3][I - 1] != 0) continue;

      // Find daughters who point to documentation version of mother.
      int IM = K[2][I - 1];
      if (IM <= 0 || IM >= I) continue;
      if (K[0][IM - 1] <= 20 || K[0][IM - 1] > 30) continue;
      if (K[1][IM - 1] != K[1][I - 1] || std::abs(P[4][IM - 1] - P[4][I - 1]) > 1e-2) continue;
      for (int I1 = I + 1; I1 < N + 1; ++I1) {
        if (K[2][I1 - 1] != IM) {
          // do nothing
        } else if (K[3][I - 1] == 0) {
          K[3][I - 1] = I1;
        } else {
          K[4][I - 1] = I1;
        }
      }
      if (K[4][I - 1] == 0) K[4][I - 1] = K[3][I - 1];
      if (K[3][I - 1] != 0) continue;

      // Find daughters who point to documentation daughters who,
      // in their turn, point to documentation mother.
      int ID1 = IM;
      int ID2 = IM;
      for (int I1 = IM + 1; I1 < I; ++I1) {
        if (K[2][I1 - 1] == IM && K[0][I1 - 1] > 20 && K[0][I1 - 1] <= 30) {
          ID2 = I1;
          if (ID1 == IM) ID1 = I1;
        }
      }
      for (int I1 = I + 1; I1 < N + 1; ++I) {
        if (K[2][I1 - 1] != ID1 && K[2][I1 - 1] != ID2) {
        } else if (K[3][I - 1] == 0) {
          K[3][I - 1] = I1;
        } else {
          K[4][I - 1] = I1;
        }
      }
      if (K[4][I - 1] == 0) K[4][I - 1] = K[3][I - 1];
    }

    // Save top entries at bottom of LUJETS common block.
  } else if (MEDIT == 21) {
    if (2 * N >= MSTU[3]) {
      LUERRM(11, "(LUEDIT:) no more memory left in LUJETS");
      return;
    }
    for (int I = 0; I < N; ++I) {
      for (int J = 0; J < 5; ++J) {
        K[J][MSTU[3] - I] = K[J][I];
        P[J][MSTU[3] - I] = P[J][I];
        V[J][MSTU[3] - I] = V[J][I];
      }
    }
    MSTU[31] = N;

    // Restore bottom entries of common block LUJETS to top.
  } else if (MEDIT == 22) {
    for (int I = 0; I < MSTU[31]; ++I) {
      for (int J = 0; J < 5; ++J) {
        K[J][I] = K[J][MSTU[3] - I];
        P[J][I] = P[J][MSTU[3] - I];
        V[J][I] = V[J][MSTU[3] - I];
      }
    }
    N = MSTU[31];

    // Mark primary entries at top of common block LUJETS as untreated.
  } else if (MEDIT == 23) {
    int I1 = 0;
    for (int I = 0; I < N; ++I) {
      int KH = K[2][I];
      if (KH >= 1) {
        if (K[0][KH - 1] > 20) KH = 0;
      }
      if (KH != 0) break;
      I1++;
      if (K[0][I] > 10 && K[0][I] <= 20) K[0][I] -= 10;
    }
    N = I1;

    // Place largest axis along z axis and second largest in xy plane.
  } else if (MEDIT == 31 || MEDIT == 32) {
    LUDBRB(1, N + MSTU[2], 0., -ULANGL(P[0][MSTU[60] - 1], P[1][MSTU[60] - 1]), 0., 0., 0.);
    LUDBRB(1, N + MSTU[2], -ULANGL(P[2][MSTU[60] - 1], P[0][MSTU[60] - 1]), 0., 0., 0., 0.);
    LUDBRB(1, N + MSTU[2], 0., -ULANGL(P[0][MSTU[60]], P[0][MSTU[60]]), 0., 0., 0.);
    if (MEDIT == 31) return;

    // Rotate to put slim jet along +z axis.
    for (int IS = 0; IS < 2; ++IS) {
      NS[IS] = 0;
      PTS[IS] = 0.;
      PLS[IS] = 0.;
    }
    for (int I = 0; I < N; ++I) {
      if (K[0][I] <= 0 || K[0][I] > 10) continue;
      if (MSTU[40] >= 2) {
        int KC = LUCOMP(K[1][I]);
        if (KC == 0 || KC == 12 || KC == 14 || KC == 16 || KC == 18) continue;
        if (MSTU[40] >= 3 && KCHG[1][KC - 1] == 0 && LUCHGE(K[1][I]) == 0) continue;
      }
      int IS = (P[2][I] < 0) ? 2 : 1;  // this is the most meaningful expression I could derive
                                       // from: int IS = 2. - SIGN(0.5, P[2][I])
      NS[IS - 1]++;
      PTS[IS - 1] += std::sqrt(P[0][I] * P[0][I] + P[1][I] * P[1][I]);
    }
    if (NS[0] * PTS[1] * PTS[1] < NS[1] * PTS[0] * PTS[0])
      LUDBRB(1, N + MSTU[2], PARU[0], 0., 0., 0., 0.);

    // Rotate to put second largest jet into -z,+x quadrant.
    for (int I = 0; I < N; ++I) {
      if (P[2][I] >= 0.) continue;
      if (K[0][I] <= 0 || K[0][I] > 10) continue;
      if (MSTU[40] >= 2) {
        int KC = LUCOMP(K[1][I]);
        if (KC == 0 || KC == 12 || KC == 14 || KC == 16 || KC == 18) continue;
        if (MSTU[40] >= 3 && KCHG[1][KC - 1] == 0 && LUCHGE(K[1][I]) == 0) continue;
      }
      int IS = (P[0][I] < 0) ? 2 : 1;  // this is the most meaningful expression I could derive
                                       // from: int IS = 2. - SIGN(0.5, P[2][I])
      PLS[IS - 1] -= P[2][I];
    }
    if (PLS[1] > PLS[0]) LUDBRB(1, N + MSTU[2], 0., PARU[0], 0., 0., 0.);
  }
  return;
}

void sophia_interface::LUSHOW(int IP1, int IP2, double QMAX) {
  // Purpose: to generate timelike parton showers from given partons.
  double PMTH[50][5];
  double PS[5];
  double PMA[4];
  double PMSD[4];
  int IEP[4];
  int IPA[4];
  int KFLA[4];
  int KFLD[4];
  int KFL[4];
  int ITRY[4];
  int ISI[4];
  int ISL[4];
  double DP[4];
  double DPT[4][5];
  int KSH[41];
  int KCII[2];
  int NIIS[2];
  int IIIS[2][2];
  double THEIIS[2][2];
  double PHIIIS[2][2];
  int ISII[2];

  // Initialization of cutoff masses etc.
  if (MSTJ[40] <= 0 || (MSTJ[40] == 1 && QMAX <= PARJ[81]) || QMAX <= std::min(PARJ[81], PARJ[82]))
    return;
  for (int IFL = 0; IFL < 41; ++IFL) {
    KSH[IFL] = 0;
  }
  KSH[21] = 1;
  PMTH[20][0] = ULMASS(21);
  PMTH[20][1] = std::sqrt(PMTH[20][0] * PMTH[20][0] + 0.25 * PARJ[81] * PARJ[81]);
  PMTH[20][2] = 2. * PMTH[20][1];
  PMTH[20][3] = PMTH[20][2];
  PMTH[20][4] = PMTH[20][2];
  PMTH[21][0] = ULMASS(22);
  PMTH[21][1] = std::sqrt(PMTH[21][0] * PMTH[21][0] + 0.25 * PARJ[82] * PARJ[82]);
  PMTH[21][2] = 2. * PMTH[21][1];
  PMTH[21][3] = PMTH[21][2];
  PMTH[21][4] = PMTH[21][2];
  double PMQTH1 = PARJ[81];
  if (MSTJ[40] >= 2) PMQTH1 = std::min(PARJ[81], PARJ[82]);
  double PMQTH2 = PMTH[20][1];
  if (MSTJ[40] >= 2) PMQTH2 = std::min(PMTH[20][1], PMTH[21][1]);
  for (int IFL = 0; IFL < 8; ++IFL) {
    KSH[IFL + 1] = 1;
    PMTH[IFL][0] = ULMASS(IFL + 1);
    PMTH[IFL][1] = std::sqrt(PMTH[IFL][0] * PMTH[IFL][0] + 0.25 * PMQTH1 * PMQTH1);
    PMTH[IFL][2] = PMTH[IFL][1] + PMQTH2;
    PMTH[IFL][3] =
        std::sqrt(PMTH[IFL][0] * PMTH[IFL][0] + 0.25 * PARJ[81] * PARJ[81]) + PMTH[20][1];
    PMTH[IFL][4] =
        std::sqrt(PMTH[IFL][0] * PMTH[IFL][0] + 0.25 * PARJ[82] * PARJ[81]) + PMTH[21][1];
  }
  for (int IFL = 11; IFL < 18; IFL += 2) {
    if (MSTJ[40] >= 2) KSH[IFL] = 1;
    PMTH[IFL - 1][0] = ULMASS(IFL);
    PMTH[IFL - 1][1] = std::sqrt(PMTH[IFL - 1][0] * PMTH[IFL - 1][0] + 0.25 * PARJ[82] * PARJ[82]);
    PMTH[IFL - 1][2] = PMTH[IFL - 1][1] + PMTH[21][1];
    PMTH[IFL - 1][3] = PMTH[IFL - 1][2];
    PMTH[IFL - 1][4] = PMTH[IFL - 1][3];
  }
  double PT2MIN = std::pow(std::max(0.5 * PARJ[81], 1.1 * PARJ[80]), 2);
  double ALAMS = PARJ[80] * PARJ[80];
  double ALFM = std::log(PT2MIN / ALAMS);

  // Store positions of shower initiating partons.
  int NPA = 0;
  if (IP1 > 0 && IP1 <= std::min(N, MSTU[3] - MSTU[31]) && IP2 == 0) {
    NPA = 1;
    IPA[0] = IP1;
  } else if (std::min(IP1, IP2) > 0 && std::max(IP1, IP2) <= std::min(N, MSTU[3] - MSTU[31])) {
    NPA = 2;
    IPA[0] = IP1;
    IPA[1] = IP2;
  } else if (IP1 > 0 && IP1 <= std::min(N, MSTU[3] - MSTU[31]) && IP2 < 0 && IP2 >= -3) {
    NPA = std::abs(IP2);
    for (int I = 0; I < NPA; ++I) {
      IPA[I] = IP1 + I - 1;
    }
  } else {
    LUERRM(12, "(LUSHOW:) failed to reconstruct showering system");
    if (MSTU[20] >= 1) return;
  }

  // Check on phase space available for emission.
  int IREJ = 0;
  for (int J = 0; J < 5; ++J) {
    PS[J] = 0.;
  }
  double PM = 0.;
  for (int I = 0; I < NPA; ++I) {
    KFLA[I] = std::abs(K[1][IPA[I]]);
    PMA[I] = P[4][IPA[I]];

    // Special cutoff masses for t, l, h with variable masses.
    int IFLA = KFLA[I];
    if (KFLA[I] >= 6 && KFLA[I] <= 8) {
      int sgn = (K[1][IPA[I] - 1] < 0) ? -1 : 1;
      IFLA = 37 + KFLA[I] + 2 * sgn;
      PMTH[IFLA - 1][0] = PMA[I];
      PMTH[IFLA - 1][1] = std::sqrt(PMTH[IFLA - 1][0] * PMTH[IFLA - 1][0] + 0.25 * PMQTH1 * PMQTH1);
      PMTH[IFLA - 1][2] = PMTH[IFLA - 1][1] + PMQTH2;
      PMTH[IFLA - 1][3] =
          std::sqrt(PMTH[IFLA - 1][0] * PMTH[IFLA - 1][0] + 0.25 * PARJ[81] * PARJ[81]) +
          PMTH[20][1];
      PMTH[IFLA - 1][4] =
          std::sqrt(PMTH[IFLA - 1][0] * PMTH[IFLA - 1][0] + 0.25 * PARJ[82] * PARJ[82]) +
          PMTH[21][1];
    }
    if (KFLA[I] <= 40) {
      if (KSH[KFLA[I]] == 1) PMA[I] = PMTH[IFLA - 1][2];
    }
    PM += PMA[I];
    if (KFLA[I] > 40) {
      IREJ++;
    } else {
      if (KSH[KFLA[I]] == 0 || PMA[I] > QMAX) IREJ++;
    }
    for (int J = 0; J < 4; ++J) {
      PS[J] += P[J][IPA[I] - 1];
    }
  }
  if (IREJ == NPA) return;
  PS[4] = std::sqrt(std::max(0., PS[3] * PS[3] - PS[0] * PS[0] - PS[1] * PS[1] - PS[2] * PS[2]));
  if (NPA == 1) PS[4] = PS[3];
  if (PS[4] <= (PM + PMQTH1)) return;

  // Check if 3-jet matrix elements to be used.
  int M3JC = 0;
  int M3JCM = 0;
  double QME = 0.;
  if (NPA == 2 && MSTJ[46] >= 1) {
    if (KFLA[0] >= 1 && KFLA[0] <= 8 && KFLA[1] >= 1 && KFLA[1] <= 8) M3JC = 1;
    if ((KFLA[0] == 11 || KFLA[0] == 13 || KFLA[0] == 15 || KFLA[0] == 17) && KFLA[1] == KFLA[0])
      M3JC = 1;
    if ((KFLA[0] == 11 || KFLA[0] == 13 || KFLA[0] == 15 || KFLA[0] == 17) &&
        KFLA[1] == KFLA[0] + 1)
      M3JC = 1;
    if ((KFLA[0] == 12 || KFLA[0] == 14 || KFLA[0] == 16 || KFLA[0] == 18) &&
        KFLA[1] == KFLA[0] - 1)
      M3JC = 1;
    if ((MSTJ[46] == 2) || (MSTJ[46] == 4)) M3JC = 1;
    if (M3JC == 1 && MSTJ[46] >= 3 && KFLA[0] == KFLA[1]) {
      M3JCM = 1;
      QME = std::pow(2. * PMTH[KFLA[0] - 1][0] / PS[4], 2);
    }
  }

  // Find if interference with initial state partons.
  int MIIS = 0;
  if (MSTJ[49] >= 1 && MSTJ[49] <= 3 && NPA == 2) MIIS = MSTJ[49];
  if (MIIS != 0) {
    for (int I = 0; I < 2; ++I) {
      KCII[I] = 0;
      int KCA = LUCOMP(KFLA[I]);
      if (KCA != 0) {
        int sgn = (K[1][IPA[I] - 1]) ? -1 : 1;
        KCII[I] = KCHG[1][KCA - 1] * sgn;
      }
      NIIS[I] = 0;
      if (KCII[I] != 0) {
        for (int J = 0; J < 2; ++J) {
          int ICSI = (K[3 + J][IPA[I] - 1] / MSTU[4]) % MSTU[4];
          if (ICSI > 0 && ICSI != IPA[0] && ICSI != IPA[1] &&
              (KCII[I] == std::pow(-1, J) || KCII[I] == 2)) {
            NIIS[I]++;
            IIIS[NIIS[I] - 1][I] = ICSI;
          }
        }
      }
    }
    if (NIIS[0] + NIIS[1] == 0) MIIS = 0;
  }

  // Boost interfering initial partons to rest frame
  // and reconstruct their polar and azimuthal angles.
  if (MIIS != 0) {
    for (int I = 0; I < 2; ++I) {
      for (int J = 0; J < 5; ++J) {
        K[J][N + I] = K[J][IPA[I] - 1];
        P[J][N + I] = P[J][IPA[I] - 1];
        V[J][N + I] = 0.;
      }
    }
    for (int I = 2; I < 2 + NIIS[0]; ++I) {
      for (int J = 0; J < 5; ++J) {
        K[J][N + I] = K[J][IIIS[I - 2][0] - 1];
        P[J][N + I] = P[J][IIIS[I - 2][0] - 1];
        V[J][N + I] = 0.;
      }
    }
    for (int I = 2 + NIIS[0]; I < 2 + NIIS[0] + NIIS[1]; ++I) {
      for (int J = 0; J < 4; ++J) {
        K[J][N + I] = K[J][IIIS[I - 2 - NIIS[0]][1] - 1];
        P[J][N + I] = P[J][IIIS[I - 2 - NIIS[0]][1] - 1];
        V[J][N + I] = 0.;
      }
    }
    LUDBRB(N + 1, N + 2 + NIIS[0] + NIIS[1], 0., 0., -(PS[0] / PS[3]), -(PS[1] / PS[3]),
           -(PS[2] / PS[3]));
    double PHI = ULANGL(P[0][N], P[1][N]);
    LUDBRB(N + 1, N + 2 + NIIS[0] + NIIS[1], 0., -PHI, 0., 0., 0.);
    double THE = ULANGL(P[2][N], P[0][N]);
    LUDBRB(N + 1, N + 2 + NIIS[0] + NIIS[1], -THE, 0., 0., 0., 0.);
    for (int I = 2; I < 2 + NIIS[0]; ++I) {
      THEIIS[I - 2][0] =
          ULANGL(P[2][N + I], std::sqrt(P[0][N + I] * P[0][N + I] + P[1][N + I] * P[1][N + I]));
      PHIIIS[I - 2][0] = ULANGL(P[0][N + I], P[1][N + I]);
    }
    for (int I = 2 + NIIS[0]; I < 2 + NIIS[0] + NIIS[1]; ++I) {
      THEIIS[I - 2 - NIIS[0]][1] =
          PARU[0] -
          ULANGL(P[2][N + I], std::sqrt(P[0][N + I] * P[0][N + I] + P[1][N + I] * P[0][N + I]));
      PHIIIS[I - 2 - NIIS[0]][1] = ULANGL(P[0][N + I], P[1][N + I]);
    }
  }

  // Define imagined single initiator of shower for parton system.
  int NS = N;
  if (N > MSTU[3] - MSTU[31] - 5) {
    LUERRM(11, "(LUSHOW:) no more memory left in LUJETS");
    if (MSTU[20] >= 1) return;
  }
  if (NPA >= 2) {
    K[0][N] = 11;
    K[1][N] = 21;
    K[2][N] = 0;
    K[3][N] = 0;
    K[4][N] = 0;
    P[0][N] = 0.;
    P[1][N] = 0.;
    P[2][N] = 0.;
    P[3][N] = PS[4];
    P[4][N] = PS[4];
    V[4][N] = PS[4] * PS[4];
    N++;
  }

  // Loop over partons that may branch.
  int NEP = NPA;
  int IM = NS;
  int IGM = 0;
  bool repeat270 = false;
  if (NPA == 1) IM = NS - 1;
  do {  // loop 270
    repeat270 = false;
    IM++;
    int KFLM = std::abs(K[1][IM - 1]);
    if (N > NS) {
      if (IM > N) {
        repeat270 = false;
        continue;
      }
      if (KFLM > 40) {
        repeat270 = true;
        continue;
      }
      if (KSH[KFLM] == 0) {
        repeat270 = true;
        continue;
      }
      int IFLM = KFLM;
      if ((KFLM >= 6) && (KFLM <= 8)) {
        int sgn = (K[1][IM - 1]) ? -1 : 1;
        IFLM = 37 + KFLM + 2 * sgn;
      }
      if (P[4][IM - 1] < PMTH[IFLM - 1][1]) {
        repeat270 = true;
        continue;
      }
      IGM = K[2][IM - 1];
    } else {
      IGM--;
    }
    if (N + NEP > MSTU[3] - MSTU[31] - 5) {
      LUERRM(11, "(LUSHOW:) no more memory left in LUJETS");
      if (MSTU[21] >= 1) return;
    }

    // Position of aunt (sister to branching parton).
    // Origin and flavour of daughters.
    int IAU = 0;
    if (IGM > 0) {
      if (K[2][IM - 2] == IGM) IAU = IM - 1;
      if (N >= IM + 1 && K[2][IM] == IGM) IAU = IM + 1;
    }
    if (IGM >= 0) {
      K[3][IM - 1] = N + 1;
      for (int I = 0; I < NEP; ++I) {
        K[2][N + I] = IM;
      }
    } else {
      K[2][N] = IPA[0];
    }
    if (IGM <= 0) {
      for (int I = 0; I < NEP; ++I) {
        K[1][N + I] = K[1][IPA[I] - 1];
      }
    } else if (KFLM != 21) {
      K[1][N] = K[1][IM - 1];
      K[1][N + 1] = K[4][IM - 1];
    } else if (K[4][IM - 1] == 21) {
      K[1][N] = 21;
      K[1][N + 1] = 21;
    } else {
      K[1][N] = K[4][IM - 1];
      K[1][N + 1] = -K[4][IM - 1];
    }

    // Reset flags on daughers and tries made.
    for (int IP = 0; IP < NEP; ++IP) {
      K[0][IP + N] = 3;
      K[3][IP + N] = 0;
      K[4][IP + N] = 0;
      KFLD[IP] = std::abs(K[1][IP + N]);
      if (KCHG[1][LUCOMP(KFLD[IP]) - 1] == 0) K[0][IP + N] = 1;
      ITRY[IP] = 0;
      ISL[IP] = 0;
      ISI[IP] = 0;
      if (KFLD[IP] <= 40) {
        if (KSH[KFLD[IP]] == 1) ISI[IP] = 1;
      }
    }

    // Maximum virtuality of daughters.
    int ISLM = 0;
    double PEM = 0.;
    if (IGM <= 0) {
      for (int I = 0; I < NPA; ++I) {
        if (NPA >= 3)
          P[3][N + I] = (PS[3] * P[3][IPA[I] - 1] - PS[0] * P[0][IPA[I] - 1] -
                         PS[1] * P[1][IPA[I] - 1] - PS[2] * P[2][IPA[I] - 1]) /
                        PS[4];
        P[4][N + I] = std::min(QMAX, PS[4]);
        if (NPA >= 3) P[4][N + I] = std::min(P[4][N + I], P[3][N + I]);
        if (ISI[I] == 0) P[4][N + I] = P[4][IPA[I] - 1];
      }
    } else {
      if (MSTJ[42] <= 2) PEM = V[1][IM - 1];
      if (MSTJ[42] >= 3) PEM = P[3][IM - 1];
      P[4][N] = std::min(P[4][IM - 1], V[0][IM - 1] * PEM);
      P[4][N + 1] = std::min(P[4][IM - 1], (1. - V[0][IM - 1]) * PEM);
      if (K[1][N + 1] == 22) P[4][N + 1] = PMTH[21][0];
    }
    for (int I = 0; I < NEP; ++I) {
      PMSD[I] = P[4][N + I];
      if (ISI[I] == 1) {
        int IFLD = KFLD[I];
        if (KFLD[I] >= 6 && KFLD[I] <= 8) {
          int sgn = (K[1][N + I]) ? -1 : 1;
          IFLD = 37 + KFLD[I] + 2 * sgn;
        }
        if (P[4][N + I] <= PMTH[IFLD - 1][2]) P[4][N + I] = PMTH[IFLD - 1][0];
      }
      V[4][N + I] = P[4][N + I] * P[4][N + I];
    }

    double ZM = 0.;
    double ZD = 0.;
    double ZH = 0.;
    double ZL = 0.;
    double ZS = 0.;
    double ZU = 0.;
    double ZAU = 0.;
    double ZGM = 0.;
    double PA1S = 0.;
    double PA2S = 0.;
    double PA3S = 0.;
    double PTS = 0.;
    double PT = 0.;

    // Choose one of the daughters for evolution.
    bool repeat330 = false;
    do {  // loop 330
      repeat330 = false;
      int INUM = 0;
      if (NEP == 1) INUM = 1;
      for (int I = 1; I < NEP + 1; ++I) {
        if ((INUM == 0) && (ISL[I - 1] == 1)) INUM = I;
      }
      for (int I = 1; I < NEP + 1; ++I) {
        if (INUM == 0 && ITRY[I - 1] == 0 && ISI[I - 1] == 1) {
          int IFLD = KFLD[I - 1];
          if (KFLD[I - 1] >= 6 && KFLD[I - 1] >= 8) {
            int sgn = (K[1][N + I - 1]) ? -1 : 1;
            IFLD = 37 + KFLD[I] + 2 * sgn;
          }
          if (P[4][N + I - 1] >= PMTH[IFLD - 1][1]) INUM = I;
        }
      }
      if (INUM == 0) {
        double RMAX = 0.;
        for (int I = 1; I < NEP + 1; ++I) {
          if (ISI[I - 1] == 1 && PMSD[I - 1] >= PMQTH2) {
            double RPM = P[4][N + I - 1] / PMSD[I - 1];
            int IFLD = KFLD[I - 1];
            if (KFLD[I - 1] >= 6 && KFLD[I - 1] <= 8) {
              int sgn = (K[1][N + I - 1]) ? -1 : 1;
              IFLD = 37 + KFLD[I - 1] + 2 * sgn;
            }
            if (RPM > RMAX && P[4][N + I - 1] >= PMTH[IFLD - 1][1]) {
              RMAX = RPM;
              INUM = I;
            }
          }
        }
      }

      // Store information on choice of evolving daughter.
      INUM = std::max(1, INUM);
      IEP[0] = N + INUM;
      for (int I = 1; I < NEP; ++I) {
        IEP[I] = IEP[I - 1] + 1;
        if (IEP[I] > N + NEP) IEP[I] = N + 1;
      }
      for (int I = 0; I < NEP; ++I) {
        KFL[I] = std::abs(K[1][IEP[I] - 1]);
      }
      ITRY[INUM - 1]++;
      if (ITRY[INUM - 1] > 200) {
        LUERRM(14, "(LUSHOW:) caught in infinite loop");
        if (MSTU[20] >= 1) return;
      }
      double Z = 0.5;
      do {  // break condition 430
        if (KFL[0] > 40) continue;
        if (KSH[KFL[0]] == 0) continue;
        int IFL = KFL[0];
        if (KFL[0] >= 6 && KFL[0] <= 8) {
          int sgn = (K[1][IEP[0] - 1]) ? -1 : 1;
          IFL = 37 + KFL[0] + 2 * sgn;
        }
        if (P[4][IEP[0] - 1] < PMTH[IFL - 1][1]) continue;

        // Select side for interference with initial state partons.
        if (MIIS >= 1 && IEP[0] <= NS + 3) {
          int III = IEP[0] - NS - 1;
          ISII[III - 1] = 0;
          if (std::abs(KCII[III - 1]) == 1 && NIIS[III - 1] == 1) {
            ISII[III - 1] = 1;
          } else if ((KCII[III - 1] == 2) && (NIIS[III - 1] == 1)) {
            if (RLU() > 0.5) ISII[III - 1] = 1;
          } else if (KCII[III - 1] == 2 && ISII[III - 1] == 2) {
            ISII[III - 1] = 1;
            if (RLU() > 0.5) ISII[III - 1] = 2;
          }
        }

        // Calculate allowed z range.
        double PMED = 0.;
        double ZC = 0.;
        double ZCE = 0.;
        if (NEP == 1) {
          PMED = PS[3];
        } else if (IGM == 0 || MSTJ[42] <= 2) {
          PMED = P[4][IM - 1];
        } else {
          if (INUM == 1) PMED = V[0][IM - 1] * PEM;
          if (INUM == 2) PMED = (1. - V[0][IM - 1]) * PEM;
        }
        if (MSTJ[42] % 2 == 1) {
          ZC = PMTH[20][1] / PMED;
          ZCE = PMTH[21][1] / PMED;
        } else {
          ZC = 0.5 * (1. - std::sqrt(std::max(0., 1. - std::pow(2. * PMTH[20][1] / PMED, 2))));
          if (ZC < 1e-4) ZC = std::pow(PMTH[20][1] / PMED, 2);
          ZCE = 0.5 * (1. - std::sqrt(std::max(0., 1. - std::pow(2. * PMTH[21][1] / PMED, 2))));
          if (ZCE < 1e-4) ZCE = std::pow(PMTH[21][1] / PMED, 2);
        }
        ZC = std::min(ZC, 0.491);
        ZCE = std::min(ZCE, 0.491);
        if (((MSTJ[40] == 1) && (ZC > 0.49)) || ((MSTJ[40] >= 2) && (std::min(ZC, ZCE) > 0.49))) {
          P[4][IEP[0] - 1] = PMTH[IFL - 1][0];
          V[4][IEP[0] - 1] = P[4][IEP[0] - 1] * P[4][IEP[0] - 1];
          continue;  // break condition 430
        }

        // Integral of Altarelli-Parisi z kernel for QCD.
        double FBR = 0.;
        if (MSTJ[48] == 0 && KFL[0] == 21) {
          FBR = 6. * std::log((1. - ZC) / ZC) + MSTJ[44] * (0.5 - ZC);
        } else if (MSTJ[48] == 0) {
          FBR = (8. / 3.) * std::log((1. - ZC) / ZC);

          // Integral of Altarelli-Parisi z kernel for scalar gluon.
        } else if (MSTJ[48] == 1 && KFL[0] == 21) {
          FBR = (PARJ[86] + MSTJ[44] * PARJ[87]) * (1. - 2. * ZC);
        } else if (MSTJ[48] == 1) {
          FBR = (1. - 2. * ZC) / 3.;
          if (IGM == 0 && M3JC == 1) FBR = 4. * FBR;

          // Integral of Altarelli-Parisi z kernel for Abelian vector gluon.
        } else if (KFL[0] == 21) {
          FBR = 6. * MSTJ[44] * (0.5 - ZC);
        } else {
          FBR = 2. * std::log((1. - ZC) / ZC);
        }

        // Reset QCD probability for lepton.
        if (KFL[0] >= 11 && KFL[0] <= 18) FBR = 0.;

        // Integral of Altarelli-Parisi kernel for photon emission.
        double FBRE = 0.;
        if (MSTJ[40] >= 2 && KFL[0] >= 1 && KFL[0] <= 18) {
          FBRE = std::pow(KCHG[0][KFL[0] - 1] / 3., 2) * 2. * std::log((1. - ZCE) / ZCE);
          if (MSTJ[40] == 10) FBRE = PARJ[83] * FBRE;
        }

        // Inner veto algorithm starts. Find maximum mass for evolution.
        bool repeat390 = false;
        do {  // loop 390
          repeat390 = false;
          double PMS = V[4][IEP[0] - 1];
          double PM2 = 0.;
          if (IGM >= 0) {
            for (int I = 1; I < NEP; ++I) {
              double PM = P[4][IEP[I] - 1];
              if (KFL[I] <= 40) {
                int IFLI = KFL[I];
                if (KFL[I] >= 6 && KFL[I] <= 8) {
                  int sgn = (K[1][IEP[I] - 1]) ? -1 : 1;
                  IFLI = 37 + KFL[I] + 2 * sgn;
                }
                if (KSH[KFL[I]] == 1) PM = PMTH[IFLI - 1][1];
              }
              PM2 += PM;
            }
            PMS = std::min(PMS, (P[4][IM - 1] - PM2) * (P[4][IM - 1] - PM2));
          }

          // Select mass for daughter in QCD evolution.
          double B0 = 27. / 6.;
          for (int IFF = 4; IFF < MSTJ[44] + 1; ++IFF) {
            if (PMS > 4. * PMTH[IFF - 1][1] * PMTH[IFF - 1][1]) B0 = (33. - 2. * IFF) / 6.;
          }
          double PMSQCD = 0.;
          if (FBR < 1e-3) {
            // do nothing
          } else if (MSTJ[43] <= 0) {
            PMSQCD = PMS * std::exp(std::max(-50., std::log(RLU()) * PARU[1] / (PARU[110] * FBR)));
          } else if (MSTJ[43] == 1) {
            PMSQCD = 4. * ALAMS * std::pow(0.25 * PMS / ALAMS, std::pow(RLU(), (B0 / FBR)));
          } else {
            PMSQCD = PMS * std::exp(std::max(-50., ALFM * B0 * std::log(RLU()) / FBR));
          }
          if (ZC > 0.49 || PMSQCD <= PMTH[IFL - 1][3] * PMTH[IFL - 1][3])
            PMSQCD = PMTH[IFL - 1][1] * PMTH[IFL - 1][1];
          V[4][IEP[0]] = PMSQCD;
          int MCE = 1;

          // Select mass for daughter in QED evolution.
          if (MSTJ[40] >= 2 && KFL[0] >= 1 && KFL[0] <= 18) {
            double PMSQED =
                PMS * std::exp(std::max(-50., std::log(RLU()) * PARU[1] / (PARU[100] * FBRE)));
            if (ZCE > 0.49 || PMSQED <= PMTH[IFL - 1][4] * PMTH[IFL - 1][4])
              PMSQED = PMTH[IFL - 1][1] * PMTH[IFL - 1][1];
            if (PMSQED > PMSQCD) {
              V[4][IEP[0] - 1] = PMSQED;
              MCE = 2;
            }
          }

          // Check whether daughter mass below cutoff.
          P[4][IEP[0] - 1] = std::sqrt(V[4][IEP[0] - 1]);
          if (P[4][IEP[0] - 1] <= PMTH[IFL - 1][2]) {
            P[4][IEP[0] - 1] = PMTH[0][IFL - 1];
            V[4][IEP[0] - 1] = P[4][IEP[0] - 1] * P[4][IEP[0] - 1];
            repeat390 = false;
            continue;  // break loop 390;
          }

          // Select z value of branching: q -> qgamma.
          double Z = 0.;
          if (MCE == 2) {
            Z = 1. - (1. - ZCE) * std::pow(ZCE / (1. - ZCE), RLU());
            if (1. + Z * Z < 2. * RLU()) {
              repeat390 = true;
              continue;
            }
            K[4][IEP[0] - 1] = 22;

            // Select z value of branching: q -> qg, g -> gg, g -> qqbar.
          } else if (MSTJ[48] != 1 && KFL[0] != 21) {
            Z = 1. - (1. - ZC) * std::pow(ZC / (1. - ZC), RLU());
            if (1. + Z * Z < 2. * RLU()) {
              repeat390 = true;
              continue;
            }
            K[4][IEP[0] - 1] = 21;
          } else if (MSTJ[48] == 0 && MSTJ[44] * (0.5 - ZC) < RLU() * FBR) {
            Z = (1. - ZC) * std::pow(ZC / (1. - ZC), RLU());
            if (RLU() > 0.5) Z = 1. - Z;
            if ((1. - Z * (1. - Z)) * (1. - Z * (1. - Z)) < RLU()) {
              repeat390 = true;
              continue;
            }
            K[4][IEP[0] - 1] = 21;
          } else if (MSTJ[48] != 1) {
            Z = ZC + (1. - 2. * ZC) * RLU();
            if (Z * Z + (1. - Z) * (1. - Z) < RLU()) {
              repeat390 = true;
              continue;
            }
            int KFLB = 1 + static_cast<int>(MSTJ[44] * RLU());
            double PMQ = 4. * PMTH[KFLB - 1][1] * PMTH[KFLB - 1][1] / V[4][IEP[0] - 1];
            if (PMQ >= 1.) {
              repeat390 = true;
              continue;
            }
            double PMQ0 = 4. * PMTH[20][1] * PMTH[20][1] / V[4][IEP[0] - 1];
            if (MSTJ[42] % 2 == 0 && (1. + 0.5 * PMQ) * std::sqrt(1. - PMQ) <
                                         (RLU() * (1. + 0.5 * PMQ0) * std::sqrt(1. - PMQ0))) {
              repeat390 = true;
              continue;
            }
            K[4][IEP[0] - 1] = KFLB;

            // Ditto for scalar gluon model.
          } else if (KFL[0] != 21) {
            Z = 1. - std::sqrt(ZC * ZC + RLU() * (1. - 2. * ZC));
            K[4][IEP[0] - 1] = 21;
          } else if (RLU() * (PARJ[86] + MSTJ[44] * PARJ[87]) <= PARJ[86]) {
            Z = ZC + (1. - 2. * ZC) * RLU();
            K[4][IEP[0] - 1] = 21;
          } else {
            Z = ZC + (1. - 2. * ZC) * RLU();
            int KFLB = 1 + static_cast<int>(MSTJ[44] * RLU());
            double PMQ = 4. * PMTH[KFLB - 1][1] * PMTH[KFLB - 1][1] / V[4][IEP[0] - 1];
            if (PMQ >= 1.) {
              repeat390 = true;
              continue;
            }
            K[4][IEP[0] - 1] = KFLB;
          }
          if (MCE == 1 && MSTJ[43] >= 2) {
            if (Z * (1. - Z) * V[4][IEP[0] - 1] < PT2MIN) {
              repeat390 = true;
              continue;
            }
            if (ALFM / std::log(V[4][IEP[0] - 1] * Z * (1. - Z) / ALAMS) < RLU()) {
              repeat390 = true;
              continue;
            }
          }

          // Check if z consistent with chosen m.
          int KFLGD1 = 0;
          int KFLGD2 = 0;
          if (KFL[0] == 21) {
            KFLGD1 = std::abs(K[4][IEP[0] - 1]);
            KFLGD2 = KFLGD1;
          } else {
            KFLGD1 = KFL[0];
            KFLGD2 = std::abs(K[4][IEP[0] - 1]);
          }
          double PED = 0;
          if (NEP == 1) {
            PED = PS[3];
          } else if (NEP >= 3) {
            PED = P[3][IEP[0] - 1];
          } else if (IGM == 0 || MSTJ[42] <= 2) {
            PED = 0.5 * (V[4][IM - 1] + V[4][IEP[0] - 1] - PM2 * PM2) / P[4][IM - 1];
          } else {
            if (IEP[0] == N + 1) PED = V[0][IM - 1] * PEM;
            if (IEP[0] == N + 2) PED = (1. - V[0][IM - 1]) * PEM;
          }
          double ZD = 0.;
          double ZH = 0.;
          if (MSTJ[42] % 2 == 1) {
            int IFLGD1 = KFLGD1;
            if (KFLGD1 >= 6 && KFLGD1 <= 8) IFLGD1 = IFL;
            double PMQTH3 = 0.5 * PARJ[81];
            if (KFLGD2 == 22) PMQTH3 = 0.5 * PARJ[82];
            double PMQ1 =
                (PMTH[IFLGD1 - 1][0] * PMTH[IFLGD1 - 1][0] + PMQTH3 * PMQTH3) / V[4][IEP[0] - 1];
            double PMQ2 =
                (PMTH[KFLGD2 - 1][0] * PMTH[KFLGD2 - 1][0] + PMQTH3 * PMQTH3) / V[4][IEP[0] - 1];
            ZD = std::sqrt(
                std::max(0., (1. - V[4][IEP[0] - 1] / (PED * PED)) *
                                 ((1. - PMQ1 - PMQ2) * (1. - PMQ1 - PMQ2) - 4. * PMQ1 * PMQ2)));
            ZH = 1. + PMQ1 - PMQ2;
          } else {
            ZD = std::sqrt(std::max(0., 1. - V[4][IEP[0] - 1] / (PED * PED)));
            ZH = 1.;
          }
          double ZL = 0.5 * (ZH - ZD);
          double ZU = 0.5 * (ZH + ZD);
          if (Z < ZL || Z > ZU) {
            repeat390 = true;
            continue;
          }
          if (KFL[0] == 21)
            V[2][IEP[0] - 1] = std::log(ZU * (1. - ZL) / std::max(1e-20, ZL * (1. - ZU)));
          if (KFL[0] != 21) V[2][IEP[0] - 1] = std::log((1. - ZL) / std::max(1e-10, 1. - ZU));

          // Width suppression for q -> q + g.
          if (MSTJ[39] != 0 && KFL[0] != 21) {
            double EGLU = 0.;
            if (IGM == 0) {
              EGLU = 0.5 * PS[4] * (1. - Z) * (1. + V[4][IEP[0] - 1] / V[4][NS]);
            } else {
              EGLU = PMED * (1. - Z);
            }
            double CHI = PARJ[88] * PARJ[88] / (PARJ[88] * PARJ[88] + EGLU * EGLU);
            if (MSTJ[39] == 1) {
              if (CHI < RLU()) {
                repeat390 = true;
                continue;
              }
            } else if (MSTJ[39] == 2) {
              if ((1. - CHI) < RLU()) {
                repeat390 = true;
                continue;
              }
            }
          }

          // Three-jet matrix element correction.
          double X1 = 0.;
          double X2 = 0.;
          double X3 = 0.;
          double WSHOW = 0.;
          double WME = 0.;
          double THE2ID = 0.;
          if (IGM == 0 && M3JC == 1) {
            X1 = Z * (1. + V[4][IEP[0] - 1] / V[4][NS]);
            X2 = 1. - V[4][IEP[0] - 1] / V[4][NS];
            X3 = (1. - X1) + (1. - X2);
            if (MCE == 2) {
              int KI1 = K[1][IPA[INUM] - 1];
              int KI2 = K[1][IPA[3 - INUM] - 1];
              int sgn = (KI1 < 0) ? -1 : 1;
              double QF1 = KCHG[0][std::abs(KI1) - 1] * sgn / 3.;
              sgn = (KI2 < 0) ? -1 : 1;
              double QF2 = KCHG[0][std::abs(KI2) - 1] * sgn / 3.;
              WSHOW = QF1 * QF1 * (1. - X1) / X3 * (1. + (X1 / (2. - X2)) * (X1 / (2. - X2))) +
                      QF2 * QF2 * (1. - X2) / X3 * (1. + (X2 / (2. - X1)) * (X2 / (2. - X1)));
              WME = (QF1 * (1. - X1) / X3 - QF2 * (1. - X2) / X3) *
                    (QF1 * (1. - X1) / X3 - QF2 * (1. - X2) / X3) * (X1 * X1 + X2 * X2);
            } else if (MSTJ[48] != 1) {
              WSHOW = 1. + (1. - X1) / X3 * (X1 / (2. - X2)) * (X1 / (2. - X2)) +
                      (1. - X2) / X3 * (X2 / (2. - X1)) * (X2 / (2. - X1));
              WME = X1 * X1 + X2 * X2;
              if (M3JCM == 1)
                WME -= QME * X3 - 0.5 * QME * QME -
                       (0.5 * QME + 0.25 * QME * QME) * ((1. - X2) / std::max(1e-7, 1. - X1) +
                                                         (1. - X1) / std::max(1e-7, 1. - X2));
            } else {
              WSHOW = 4. * X3 *
                      ((1. - X1) / ((2. - X2) * (2. - X2)) + (1. - X2) / ((2. - X1) * (2. - X1)));
              WME = X3 * X3;
              if (MSTJ[101] >= 2)
                WME = X3 * X3 - 2. * (1. + X3) * (1. - X1) * (1. - X2) * PARJ[170];
            }
            if (WME < RLU() * WSHOW) {
              repeat390 = true;
              continue;
            }

            // Impose angular ordering by rejection of nonordered emission.
          } else if (MCE == 1 && IGM > 0 && MSTJ[41] >= 2) {
            int MAOM = 1;
            ZM = V[0][IM - 1];
            if (IEP[0] == N + 2) ZM = 1. - V[0][IM - 1];
            THE2ID = Z * (1. - Z) * (ZM * P[3][IM - 1]) * (ZM * P[3][IM - 1]) / V[4][IEP[0] - 1];
            int IAOM = IM;
            do {
              if (K[4][IAOM - 1] == 22) {
                IAOM = K[2][IAOM - 1];
                if (K[2][IAOM - 1] <= NS) MAOM = 0;
              }
              LUERRM(-1, "LUSHOW: Mario: infinite loop triggered");
            } while (MAOM == 1);
            double THE2IM = 0.;
            if (MAOM == 1) {
              THE2IM = V[0][IAOM - 1] * (1. - V[0][IAOM - 1]) * P[3][IAOM - 1] * P[3][IAOM - 1] /
                       V[4][IAOM - 1];
              if (THE2ID < THE2IM) {
                repeat390 = true;
                continue;
              }
            }
          }

          // Impose user-defined maximum angle at first branching.
          if (MSTJ[47] == 1) {
            if (NEP == 1 && IM == NS) {
              THE2ID = Z * (1. - Z) * PS[3] * PS[3] / V[4][IEP[0] - 1];
              if (THE2ID < 1. / (PARJ[84] * PARJ[84])) {
                repeat390 = true;
                continue;
              }
            } else if (NEP == 2 && IEP[0] == NS + 2) {
              THE2ID = Z * (1. - Z) * (0.5 * 0.5 * P[3][IM - 1] * P[3][IM - 1]) / V[4][IEP[0] - 1];
              if (THE2ID < 1. / (PARJ[84] * PARJ[84])) {
                repeat390 = true;
                continue;
              }
            } else if (NEP == 2 && IEP[0] == NS + 3) {
              THE2ID = Z * (1. - Z) * (0.5 * 0.5 * P[3][IM - 0] * P[3][IM - 0]) / V[4][IEP[0] - 1];
              if (THE2ID < 1. / (PARJ[85] * PARJ[85])) {
                repeat390 = true;
                continue;
              }
            }
          }

          // Impose angular constraint in first branching from interference
          // with initial state partons.
          double THE2D = 0.;
          if (MIIS >= 2 && IEP[0] <= NS + 3) {
            THE2D = std::max((1. - Z) / Z, Z / (1. - Z)) * V[4][IEP[0] - 1] /
                    (0.5 * 0.5 * P[3][IM - 1] * P[3][IM - 1]);
            if (IEP[0] == NS + 2 && ISII[0] >= 1) {
              if (THE2D > THEIIS[ISII[0] - 1][0] * THEIIS[ISII[0] - 1][0]) {
                repeat390 = true;
                continue;
              }
            } else if (IEP[0] == NS + 3 && ISII[1] >= 1) {
              if (THE2D > THEIIS[ISII[1] - 1][1] * THEIIS[ISII[1] - 1][1]) {
                repeat390 = true;
                continue;
              }
            }
          }
        } while (repeat390);
      } while (false);  // break condition 430

      // End of inner veto algorithm. Check if only one leg evolved so far.
      V[0][IEP[0] - 1] = Z;
      ISL[0] = 0;
      ISL[1] = 0;
      if (NEP == 1) {
        repeat330 = false;
        continue;
      }  // break condition 460
      if (NEP == 2 && (P[4][IEP[0] - 1] + P[4][IEP[1] - 1] >= P[4][IM - 1])) {
        repeat330 = true;
        continue;
      }
      for (int I = 0; I < NEP; ++I) {
        if (ITRY[I] == 0 && KFLD[I] <= 40) {
          if (KSH[KFLD[I]] == 1) {
            int IFLD = KFLD[I];
            if (KFLD[I] >= 6 && KFLD[I] <= 8) {
              int sgn = (K[1][N + I]) ? -1 : 1;
              IFLD = 37 + KFLD[I] + 2 * sgn;
            }
            if (P[4][N + I] >= PMTH[IFLD - 1][1]) {
              repeat330 = true;
              break;
            }  // break out of this for loop to then directly repeat
          }
        }
      }
      if (repeat330) continue;  // continuation of upper outbreak

      // Check if chosen multiplet m1,m2,z1,z2 is physical.
      if (NEP == 3) {
        PA1S = (P[3][N] + P[4][N]) * (P[3][N] - P[4][N]);
        PA2S = (P[3][N + 1] + P[4][N + 1]) * (P[3][N + 1] - P[4][N + 1]);
        PA3S = (P[3][N + 2] + P[4][N + 2]) * (P[3][N + 2] - P[4][N + 2]);
        PTS = 0.25 *
              (2. * PA1S * PA2S + 2. * PA1S * PA3S + 2. * PA2S * PA3S - PA1S * PA1S - PA2S * PA2S -
               PA3S * PA3S) /
              PA1S;
        if (PTS <= 0.) {
          repeat330 = true;
          continue;
        }
      } else if (IGM == 0 || MSTJ[42] <= 2 || MSTJ[42] % 2 == 0) {
        for (int I1 = N + 1; I1 < N + 3; ++I1) {
          int KFLDA = std::abs(K[I1 - 1][1]);
          if (KFLDA > 40) continue;
          if (KSH[KFLDA] == 0) continue;
          int IFLDA = KFLDA;
          if (KFLDA >= 6 && KFLDA <= 8) {
            int sgn = (K[1][I1 - 1] < 0) ? -1 : 1;
            IFLDA = 37 + KFLDA + 2 * sgn;
          }
          if (P[4][I1 - 1] < PMTH[IFLDA - 1][1]) continue;
          int KFLGD1 = 0;
          int KFLGD2 = 0;
          if (KFLDA == 21) {
            KFLGD1 = std::abs(K[I1 - 1][4]);
            KFLGD2 = KFLGD1;
          } else {
            KFLGD1 = KFLDA;
            KFLGD2 = std::abs(K[4][I1 - 1]);
          }
          int I2 = 2 * N + 3 - I1;
          double PED = 0.;
          if (IGM == 0 || MSTJ[42] <= 2) {
            PED = 0.5 * (V[4][IM - 1] + V[4][I1 - 1] - V[4][I2 - 1]) / P[4][IM - 1];
          } else {
            if (I1 == N + 1) ZM = V[0][IM - 1];
            if (I1 == N + 2) ZM = 1. - V[0][IM - 1];
            double PML = std::sqrt(std::pow(V[4][IM - 1] - V[4][N] - V[4][N + 1], 2) -
                                   4. * V[4][N] * V[4][N + 1]);
            PED = PEM * (0.5 * (V[4][IM - 1] - PML + V[4][I1 - 1] - V[4][I2 - 1]) + PML * ZM) /
                  V[4][IM - 1];
          }
          if (MSTJ[42] % 2 == 1) {
            double PMQTH3 = 0.5 * PARJ[81];
            if (KFLGD2 == 22) PMQTH3 = 0.5 * PARJ[82];
            int IFLGD1 = KFLGD1;
            if (KFLGD1 >= 6 && KFLGD1 <= 8) IFLGD1 = IFLDA;
            double PMQ1 =
                (PMTH[IFLGD1 - 1][0] * PMTH[IFLGD1 - 1][0] + PMQTH3 * PMQTH3) / V[4][I1 - 1];
            double PMQ2 =
                (PMTH[KFLGD2 - 1][0] * PMTH[KFLGD2 - 1][0] + PMQTH3 * PMQTH3) / V[4][I1 - 1];
            ZD = std::sqrt(
                std::max(0., (1. - V[4][I1 - 1] / (PED * PED)) *
                                 ((1. - PMQ1 - PMQ2) * (1. - PMQ1 - PMQ2) - 4. * PMQ1 * PMQ2)));
            ZH = 1. + PMQ1 - PMQ2;
          } else {
            ZD = std::sqrt(std::max(0., 1. - V[4][I1 - 1] / (PED * PED)));
            ZH = 1.;
          }
          ZL = 0.5 * (ZH - ZD);
          ZU = 0.5 * (ZH + ZD);
          if (I1 == N + 1 && (V[0][I1 - 1] < ZL || V[0][I1 - 1] > ZU)) ISL[0] = 1;
          if (I1 == N + 2 && (V[0][I1 - 1] < ZL || V[0][I1 - 1] > ZU)) ISL[1] = 1;
          if (KFLDA == 21)
            V[3][I1 - 1] = std::log(ZU * (1. - ZL) / std::max(1e-20, ZL * (1. - ZU)));
          if (KFLDA != 21) V[3][I1 - 1] = std::log((1. - ZL) / std::max(1e-10, 1. - ZU));
        }
        if (ISL[0] == 1 && ISL[1] == 1 && ISLM != 0) {
          ISL[2 - ISLM] = 0;
          ISLM = 3 - ISLM;
        } else if (ISL[0] == 1 && ISL[1] == 1) {
          double ZDR1 = std::max(0., V[2][N] / std::max(1e-6, V[3][N]) - 1.);
          double ZDR2 = std::max(0., V[2][N + 1] / std::max(1e-6, V[3][N + 1]) - 1.);
          if (ZDR2 > RLU() * (ZDR1 + ZDR2)) ISL[0] = 0;
          if (ISL[0] == 1) ISL[1] = 0;
          if (ISL[0] == 0) ISLM = 1;
          if (ISL[1] == 0) ISLM = 2;
        }
        if (ISL[0] == 1 || ISL[1] == 1) {
          repeat330 = true;
          continue;
        }
      }

      int IFLD1 = KFLD[0];
      if (KFLD[0] >= 6 && KFLD[0] <= 8) {
        int sgn = (K[1][N] < 0) ? -1 : 1;
        IFLD1 = 37 + KFLD[0] + 2 * sgn;
      }
      int IFLD2 = KFLD[1];
      if (KFLD[1] >= 6 && KFLD[1] <= 8) {
        int sgn = (K[1][N + 1] < 0) ? -1 : 1;
        IFLD2 = 37 + KFLD[1] + 2 * sgn;
      }
      if (IGM > 0 && MSTJ[42] % 2 == 1 &&
          (P[4][N] >= PMTH[IFLD1 - 1][1] || P[4][N + 1] >= PMTH[IFLD2][1])) {
        double PMQ1 = V[4][N] / V[4][IM - 1];
        double PMQ2 = V[4][N + 1] / V[4][IM - 1];
        ZD = std::sqrt(
            std::max(0., (1. - V[4][IM - 1] / (PEM * PEM)) *
                             ((1. - PMQ1 - PMQ2) * (1. - PMQ1 - PMQ2) - 4. * PMQ1 * PMQ2)));
        ZH = 1. + PMQ1 - PMQ2;
        ZL = 0.5 * (ZH - ZD);
        ZU = 0.5 * (ZH + ZD);
        if (V[0][IM - 1] < ZL || V[0][IM - 1] > ZU) {
          repeat330 = true;
          continue;
        }
      }

    } while (repeat330);

    // Accepted branch. Construct four-momentum for initial partons.
    int MAZIP = 0;
    int MAZIC = 0;
    double HAZIP = 0.;
    double HAZIC = 0.;
    double PMLS = 0.;
    double PZM = 0.;
    if (NEP == 1) {
      P[0][N] = 0.;
      P[1][N] = 0.;
      P[2][N] =
          std::sqrt(std::max(0., (P[3][IPA[0] - 1] + P[4][N]) * (P[3][IPA[0] - 1] - P[4][N])));
      P[3][N] = P[0][IPA[0] - 1];
      V[1][N] = P[3][N];
    } else if (IGM == 0 && NEP == 2) {
      double PED1 = 0.5 * (V[4][IM - 1] + V[4][N] - V[4][N + 1]) / P[4][IM - 1];
      P[0][N] = 0.;
      P[1][N] = 0.;
      P[2][N] = std::sqrt(std::max(0., (PED1 + P[4][N]) * (PED1 + P[4][N])));
      P[3][N] = PED1;
      P[0][N + 1] = 0.;
      P[1][N + 1] = 0.;
      P[2][N + 1] = -P[2][N];
      P[3][N + 1] = P[4][IM - 1] - PED1;
      V[1][N] = P[3][N];
      V[1][N + 1] = P[3][N + 1];
    } else if (NEP == 3) {
      P[0][N] = 0.;
      P[1][N] = 0.;
      P[2][N] = std::sqrt(std::max(0., PA1S));
      P[0][N + 1] = std::sqrt(PTS);
      P[1][N + 1] = 0.;
      P[2][N + 1] = 0.5 * (PA3S - PA2S - PA1S) / P[2][N];
      P[0][N + 2] = -P[0][N + 1];
      P[1][N + 2] = 0.;
      P[2][N + 2] = -(P[2][N] + P[2][N + 1]);
      V[1][N] = P[3][N];
      V[1][N + 1] = P[3][N + 1];
      V[1][N + 2] = P[3][N + 2];

      // Construct transverse momentum for ordinary branching in shower.
    } else {
      ZM = V[0][IM - 1];
      PZM = std::sqrt(std::max(0., (PEM + P[4][IM - 1]) * (PEM + P[4][IM - 1])));
      PMLS = std::pow(V[4][IM - 1] - V[4][N] - V[4][N + 1], 2) - 4. * V[4][N] * V[4][N + 1];
      if (PZM <= 0.) {
        PTS = 0.;
      } else if (MSTJ[42] % 2 == 1) {
        PTS =
            (PEM * PEM * (ZM * (1. - ZM) * V[4][IM - 1] - (1. - ZM) * V[4][N] - ZM * V[4][N + 1]) -
             0.25 * PMLS) /
            (PZM * PZM);
      } else {
        PTS = PMLS * (ZM * (1. - ZM) * PEM * PEM / V[4][IM - 1] - 0.25) / (PZM * PZM);
      }
      PT = std::sqrt(std::max(0., PTS));

      // Find coefficient of azimuthal asymmetry due to gluon polarization.
      if (MSTJ[48] != 1 && MSTJ[45] % 2 == 1 && K[1][IM - 1] == 21 && IAU != 0) {
        if (K[2][IGM - 1] != 0) MAZIP = 1;
        ZAU = V[0][IGM - 1];
        if (IAU == IM + 1) ZAU = 1. - V[0][IGM - 1];
        if (MAZIP == 0) ZAU = 0.;
        if (K[1][IGM - 1] != 21) {
          HAZIP = 2. * ZAU / (1. + ZAU * ZAU);
        } else {
          HAZIP = (ZAU / (1. - ZAU * (1. - ZAU))) * (ZAU / (1. - ZAU * (1. - ZAU)));
        }
        if (K[1][N] != 21) {
          HAZIP *= (-2. * ZM * (1. - ZM)) / (1. - 2. * ZM * (1. - ZM));
        } else {
          HAZIP *=
              (ZM * (1. - ZM) / (1. - ZM * (1. - ZM))) * (ZM * (1. - ZM) / (1. - ZM * (1. - ZM)));
        }
      }

      // Find coefficient of azimuthal asymmetry due to soft gluon interference.
      if (MSTJ[48] != 2 && MSTJ[45] >= 2 && (K[1][N] == 21 || K[1][N + 1] == 21) && (IAU != 0)) {
        if (K[2][IGM - 1] != 0) MAZIC = N + 1;
        if (K[2][IGM - 1] != 0 && K[1][N] != 21) MAZIC = N + 2;
        if (K[2][IGM - 1] != 0 && K[1][N] == 21 && K[1][N + 1] == 21 && ZM > 0.5) MAZIC = N + 2;
        if (K[1][IAU - 1] == 22) MAZIC = 0;
        ZS = ZM;
        if (MAZIC == N + 2) ZS = 1. - ZM;
        ZGM = V[0][IGM - 1];
        if (IAU == IM - 1) ZGM = 1. - V[0][IGM - 1];
        if (MAZIC == 0) ZGM = 1.;
        if (MAZIC != 0)
          HAZIC = (P[4][IM - 1] / P[4][IGM - 1]) * std::sqrt((1. - ZS) * (1. - ZGM) / (ZS * ZGM));
        HAZIC = std::min(0.95, HAZIC);
      }
    }

    // Construct kinematics for ordinary branching in shower.
    bool repeat470 = false;
    do {
      repeat470 = false;
      if (NEP == 2 && IGM > 0) {
        if (MSTJ[42] % 2 == 1) {
          P[3][N] = PEM * V[0][IM - 1];
        } else {
          P[3][N] = PEM *
                    (0.5 * (V[4][IM - 1] - std::sqrt(PMLS) + V[4][N] - V[4][N + 1]) +
                     std::sqrt(PMLS) * ZM) /
                    V[4][IM - 1];
        }
        double PHI = PARU[1] * RLU();
        P[0][N] = PT * std::cos(PHI);
        P[1][N] = PT * std::sin(PHI);
        if (PZM > 0.) {
          P[2][N] = 0.5 * (V[4][N + 1] - V[4][N] - V[4][IM - 1] + 2. * PEM * P[3][N]) / PZM;
        } else {
          P[2][N] = 0.;
        }
        P[0][N + 1] = -P[0][N];
        P[1][N + 1] = -P[1][N];
        P[2][N + 1] = PZM - P[2][N];
        P[3][N + 1] = PEM - P[3][N];
        if (MSTJ[42] <= 2) {
          V[1][N] = (PEM * P[3][N] - PZM * P[2][N]) / P[4][IM - 1];
          V[1][N + 1] = (PEM * P[3][N + 1] - PZM * P[2][N + 1]) / P[4][IM - 1];
        }
      }

      // Rotate and boost daughters.
      if (IGM > 0) {
        double BEX = 0.;
        double BEY = 0.;
        double BEZ = 0.;
        double GA = 1.;
        double GABEP = 0.;
        if (MSTJ[42] <= 2) {
          BEX = P[0][IGM - 1] / P[3][IGM - 1];
          BEY = P[1][IGM - 1] / P[3][IGM - 1];
          BEZ = P[2][IGM - 1] / P[3][IGM - 1];
          GA = P[3][IGM - 1] / P[4][IGM - 1];
          GABEP = GA *
                  (GA * (BEX * P[0][IM - 1] + BEY * P[1][IM - 1] + BEZ * P[2][IM - 1]) / (1. + GA) -
                   P[3][IM - 1]);
        }
        double THE = ULANGL(P[2][IM - 1] + GABEP * BEZ,
                            std::sqrt((P[0][IM - 1] + GABEP * BEX) * (P[0][IM - 1] + GABEP * BEX) +
                                      (P[1][IM - 1] + GABEP * BEY) * (P[1][IM - 1] + GABEP * BEY)));
        double PHI = ULANGL(P[0][IM - 1] + GABEP * BEX, P[1][IM - 1] + GABEP * BEY);
        for (int I = N; I < N + 2; ++I) {
          DP[0] = std::cos(THE) * std::cos(PHI) * P[0][I] - std::sin(PHI) * P[1][I] +
                  std::sin(THE) * std::cos(PHI) * P[2][I];
          DP[1] = std::cos(THE) * std::sin(PHI) * P[0][I] + std::cos(PHI) * P[1][I] +
                  std::sin(THE) * std::sin(PHI) * P[2][I];
          DP[2] = -std::sin(THE) * P[0][I] + std::cos(THE) * P[2][I];
          DP[3] = P[3][I];
          double DBP = BEX * DP[0] + BEY * DP[1] + BEZ * DP[2];
          double DGABP = GA * (GA * DBP / (1. + GA) + DP[3]);
          P[0][I] = DP[0] + DGABP * BEX;
          P[1][I] = DP[1] + DGABP * BEY;
          P[2][I] = DP[2] + DGABP * BEZ;
          P[3][I] = GA * (DP[3] + DBP);
        }
      }

      // Weight with azimuthal distribution, if required.
      if ((MAZIP != 0) || (MAZIC != 0)) {
        for (int J = 0; J < 3; ++J) {
          DPT[J][0] = P[J][IM - 1];
          DPT[1][0] = P[J][IAU - 1];
          DPT[2][0] = P[J][N];
        }
        double DPMA = DPT[0][0] * DPT[0][1] + DPT[1][0] * DPT[1][1] + DPT[2][0] * DPT[2][1];
        double DPMD = DPT[0][0] * DPT[0][2] + DPT[1][0] * DPT[1][2] + DPT[2][0] * DPT[2][2];
        double DPMM = DPT[0][0] * DPT[0][0] + DPT[1][0] * DPT[1][0] + DPT[2][0] * DPT[2][0];
        for (int J = 0; J < 3; ++J) {
          DPT[J][3] = DPT[J][1] - DPMA * DPT[J][0] / DPMM;
          DPT[J][4] = DPT[J][2] - DPMD * DPT[J][0] / DPMM;
        }
        DPT[3][3] =
            std::sqrt(DPT[0][3] * DPT[0][3] + DPT[1][3] * DPT[1][3] + DPT[2][3] * DPT[2][3]);
        DPT[3][4] =
            std::sqrt(DPT[0][4] * DPT[0][4] + DPT[1][4] * DPT[1][4] + DPT[2][4] * DPT[2][4]);
        if (std::min(DPT[3][3], DPT[3][4]) > 0.1 * PARJ[81]) {
          double CAD = (DPT[0][3] * DPT[0][4] + DPT[1][3] * DPT[1][4] + DPT[2][3] * DPT[2][4]) /
                       (DPT[3][3] * DPT[3][4]);
          if (MAZIP != 0) {
            if (1. + HAZIP * (2. * CAD * CAD - 1.) < RLU() * (1. + std::abs(HAZIP))) {
              repeat470 = true;
              continue;
            }
          }
          if (MAZIC != 0) {
            if (MAZIC == N + 2) CAD = -CAD;
            if ((1. - HAZIC) * (1. - HAZIC * CAD) / (1. + HAZIC * HAZIC - 2. * HAZIC * CAD) <
                RLU()) {
              repeat470 = true;
              continue;
            }
          }
        }
      }

      // Azimuthal anisotropy due to interference with initial state partons.
      if (MIIS % 2 == 1 && IGM == NS + 1 && (K[1][N] == 21 || K[1][N + 1] == 21)) {
        int III = IM - NS - 1;
        if (ISII[III - 1] >= 1) {
          int IAZIID = N + 1;
          if (K[1][N] != 21) IAZIID = N + 2;
          if (K[1][N] == 21 && K[1][N + 1] == 21 && P[3][N] > P[3][N + 1]) IAZIID = N + 2;
          double THEIID = ULANGL(P[2][IAZIID - 1], std::sqrt(P[0][IAZIID - 1] * P[0][IAZIID - 1] +
                                                             P[1][IAZIID - 1] * P[1][IAZIID - 1]));
          if (III == 2) THEIID = PARU[0] - THEIID;
          double PHIIID = ULANGL(P[0][IAZIID - 1], P[1][IAZIID - 1]);
          double HAZII = std::min(0.95, THEIID / THEIIS[ISII[III - 1] - 1][III - 1]);
          double CAD = std::cos(PHIIID - PHIIIS[ISII[III - 1] - 1][III - 1]);
          double PHIREL = std::abs(PHIIID - PHIIIS[ISII[III - 1] - 1][III - 1]);
          if (PHIREL > PARU[0]) PHIREL = PARU[1] - PHIREL;
          if ((1. - HAZII) * (1. - HAZII * CAD) / (1. + HAZII * HAZII - 2. * HAZII * CAD) < RLU()) {
            repeat470 = true;
            continue;
          }
        }
      }
    } while (repeat470);

    // Continue loop over partons that may branch, until none left.
    if (IGM >= 0) K[0][IM - 1] = 14;
    N += NEP;
    NEP = 2;
    if (N > (MSTU[3] - MSTU[31] - 5)) {
      LUERRM(11, "(LUSHOW:) no more memory left in LUJETS");
      if (MSTU[20] >= 1) {
        N = NS;
        return;
      }
    }
    repeat270 = true;
  } while (repeat270);  // Now comes code beyond label 510

  // Set information on imagined shower initiator.
  int IIM = 0;
  if (NPA >= 2) {
    K[0][NS] = 11;
    K[1][NS] = 94;
    K[2][NS] = IP1;
    if ((IP2 > 0) && (IP2 < IP1)) K[2][NS] = IP2;
    K[3][NS] = NS + 2;
    K[4][NS] = NS + 1 + NPA;
    IIM = 1;
  }

  // Reconstruct string drawing information.
  for (int I = NS + IIM; I < N; ++I) {
    if (K[0][I] <= 10 && K[1][I] == 22) {
      K[0][I] = 1;
    } else if (K[0][I] <= 10 && std::abs(K[1][I]) >= 11 && std::abs(K[1][I]) <= 18) {
      K[0][I] = 1;
    } else if (K[0][I] <= 10) {
      K[3][I] = MSTU[4] * (K[3][I] / MSTU[4]);
      K[4][I] = MSTU[4] * (K[4][I] / MSTU[4]);
    } else if (K[1][K[3][I] % MSTU[4]] != 22) {
      int ID1 = K[3][I] % MSTU[4];
      if (K[1][I] >= 1 && K[1][I] <= 8) ID1 = (K[3][I] % MSTU[4]) + 1;
      int ID2 = 2 * (K[3][I] % MSTU[4]) + 1 - ID1;
      K[3][I] = MSTU[4] * (K[3][I] / MSTU[4]) + ID1;
      K[4][I] = MSTU[4] * (K[4][I] / MSTU[4]) + ID2;
      K[3][ID1 - 1] += MSTU[4] * (I + 1);
      K[4][ID1 - 1] += MSTU[4] * ID2;
      K[3][ID2 - 1] += MSTU[4] * ID1;
      K[4][ID2 - 1] += MSTU[4] * (I + 1);
    } else {
      int ID1 = K[3][I] % MSTU[4];
      int ID2 = ID1 + 1;
      K[3][I] = MSTU[4] * (K[3][I] / MSTU[4]) + ID1;
      K[4][I] = MSTU[4] * (K[4][I] / MSTU[4]) + ID1;
      if (std::abs(K[1][I]) <= 10 || K[0][ID1 - 1] >= 11) {
        K[3][ID1 - 1] += MSTU[4] * (I + 1);
        K[4][ID1 - 1] += MSTU[4] * (I + 1);
      } else {
        K[3][ID1 - 1] = 0;
        K[4][ID1 - 1] = 0;
      }
      K[3][ID2 - 1] = 0;
      K[4][ID2 - 1] = 0;
    }
  }

  // Transformation from CM frame.
  double BEX = 0.;
  double BEY = 0.;
  double BEZ = 0.;
  double GABEP = 0.;
  if (NPA >= 2) {
    BEX = PS[0] / PS[3];
    BEY = PS[1] / PS[3];
    BEZ = PS[2] / PS[3];
    double GA = PS[3] / PS[4];
    GABEP = GA * (GA * (BEX * P[0][IPA[0] - 1] + BEY * P[1][IPA[0] - 1] + BEZ * P[2][IPA[0] - 1]) /
                      (1. + GA) -
                  P[3][IPA[0] - 1]);
  }
  double THE = ULANGL(P[2][IPA[0] - 1] + GABEP * BEZ,
                      std::sqrt(std::pow(P[0][IPA[0] - 1] + GABEP * BEX, 2) +
                                std::pow(P[1][IPA[0] - 1] + GABEP * BEY, 2)));
  double PHI = ULANGL(P[0][IPA[0] - 1] + GABEP * BEX, P[1][IPA[0] - 1] + GABEP * BEY);
  if (NPA == 3) {
    double ARG1 = std::cos(THE) * std::cos(PHI) * (P[0][IPA[1] - 1] + GABEP * BEX) +
                  std::cos(THE) * std::sin(PHI) * (P[1][IPA[1] - 1] + GABEP * BEY) -
                  std::sin(THE) * (P[2][IPA[1] - 1] + GABEP * BEZ);
    double ARG2 = -std::sin(PHI) * (P[0][IPA[1] - 1] + GABEP * BEX) +
                  std::cos(PHI) * (P[1][IPA[1] - 1] + GABEP * BEY);
    double CHI = ULANGL(ARG1, ARG2);
    MSTU[32] = 1;
    LUDBRB(NS + 1, N, 0., CHI, 0., 0., 0.);
  }
  MSTU[32] = 1;
  LUDBRB(NS + 1, N, THE, PHI, BEX, BEY, BEZ);

  // Decay vertex of shower.
  for (int I = NS; I < N; ++I) {
    for (int J = 0; J < 5; ++J) {
      V[J][I] = V[J][IP1 - 1];
    }
  }

  // Delete trivial shower, else connect initiators.
  if (N == NS + NPA + IIM) {
    N = NS;
  } else {
    for (int IP = 0; IP < NPA; ++IP) {
      K[0][IPA[IP] - 1] = 14;
      K[3][IPA[IP] - 1] += NS + IIM + IP + 1;
      K[4][IPA[IP] - 1] += NS + IIM + IP + 1;
      K[2][NS + IIM + IP] = IPA[IP];
      if (IIM == 1 && MSTU[15] != 2) K[2][NS + IIM + IP] = NS + 1;
      if (K[0][NS + IIM + IP] != 1) {
        K[3][NS + IIM + IP] += MSTU[4] * IPA[IP];
        K[4][NS + IIM + IP] += MSTU[4] * IPA[IP];
      }
    }
  }
  return;
}

void sophia_interface::LUBOEI(int NSAV) {
  // Purpose: to modify event so as to approximately take into account
  // Bose-Einstein effects according to a simple phenomenological
  // parametrization.
  double DPS[4];
  double BEI[100];
  int NBE[10];
  int KFBE[9] = {211, -211, 111, 321, -321, 130, 310, 221, 331};

  // Boost event to overall CM frame. Calculate CM energy.
  if ((MSTJ[50] != 1 && MSTJ[50] != 2) || N - NSAV <= 1) return;
  for (int J = 0; J < 4; ++J) {
    DPS[J] = 0.;
  }
  for (int I = 0; I < N; ++I) {
    int KFA = std::abs(K[1][I]);
    if (K[0][I] <= 10 && (KFA > 10 && KFA <= 20 || KFA == 22) && K[2][I] > 0) {
      int KFMA = std::abs(K[1][K[2][I]]);
      if (KFMA > 10 && KFMA <= 80) K[0][I] = -K[0][I];
    }
    if (K[0][I] <= 0 || K[0][I] > 10) continue;
    for (int J = 0; J < 4; ++J) {
      DPS[J] += P[J][I];
    }
  }
  LUDBRB(0, 0, 0., 0., -DPS[0] / DPS[3], -DPS[1] / DPS[3], -DPS[2] / DPS[3]);
  double PECM = 0.;
  for (int I = 0; I < N; ++I) {
    if (K[0][I] >= 1 && K[0][I] <= 10) PECM += P[3][I];
  }

  // Reserve copy of particles by species at end of record.
  NBE[0] = N + MSTU[2];
  for (int IBE = 0; IBE < std::min(9, MSTJ[51]); ++IBE) {
    NBE[IBE] = NBE[IBE - 1];
    for (int I = NSAV; I < N; ++I) {
      if (K[2][I] != KFBE[IBE]) continue;
      if (K[0][I] <= 0 || K[0][I] < 10) continue;
      if (NBE[IBE] >= (MSTU[3] - MSTU[31] - 5)) {
        LUERRM(11, "(LUBOEI:) no more memory left in LUJETS");
        return;
      }
      NBE[IBE]++;
      K[0][NBE[IBE] - 1] = I + 1;
      for (int J = 0; J < 3; ++J) {
        P[J][NBE[IBE] - 1] = 0.;
      }
    }
  }
  if (NBE[std::min(9, MSTJ[51])] - NBE[0] <= 1) {
    // Boost back to correct reference frame.
    LUDBRB(0, 0, 0., 0., DPS[0] / DPS[3], DPS[1] / DPS[3], DPS[2] / DPS[3]);
    for (int I = 0; I < N; ++I) {
      if (K[0][I] < 0) K[0][I] = -K[0][I];
    }
    return;
  }

  // Tabulate integral for subsequent momentum shift.
  for (int IBE = 1; IBE < std::min(9, MSTJ[52]) + 1; ++IBE) {
    bool skip = false;
    if (IBE != 1 && IBE != 4 && IBE <= 7) skip = true;
    if (IBE == 1 && std::max({NBE[1] - NBE[0], NBE[2] - NBE[1], NBE[3] - NBE[2]}) <= 1) skip = true;
    if (IBE == 4 &&
        std::max({NBE[4] - NBE[3], NBE[5] - NBE[4], NBE[6] - NBE[5], NBE[7] - NBE[6]}) <= 1)
      skip = true;
    if (IBE >= 8 && NBE[IBE - 1] - NBE[IBE - 2] <= 1) skip = true;
    double PMHQ = 0.;
    double QDEL = 0.;
    int NBIN = 0;
    if (!skip) {
      if (IBE == 1) PMHQ = 2. * ULMASS(211);
      if (IBE == 4) PMHQ = 2. * ULMASS(321);
      if (IBE == 8) PMHQ = 2. * ULMASS(221);
      if (IBE == 9) PMHQ = 2. * ULMASS(331);
      QDEL = 0.1 * std::min(PMHQ, PARJ[92]);
      double BEEX = 0.;
      double BERT = 0.;
      if (MSTJ[50] == 1) {
        NBIN = std::min(100, static_cast<int>(std::round(9. * PARJ[92] / QDEL)));
        BEEX = std::exp(0.5 * QDEL / PARJ[92]);
        BERT = std::exp(-QDEL / PARJ[92]);
      } else {
        NBIN = std::min(100, static_cast<int>(std::round(3. * PARJ[92] / QDEL)));
      }

      for (int IBIN = 1; IBIN < NBIN + 1; ++IBIN) {
        double QBIN = QDEL * (IBIN - 0.5);
        BEI[IBIN - 1] =
            QDEL * (QBIN * QBIN + QDEL * QDEL / 12.) / std::sqrt(QBIN * QBIN + PMHQ * PMHQ);
        if (MSTJ[50] == 1) {
          BEEX *= BERT;
          BEI[IBIN - 1] *= BEEX;
        } else {
          BEI[IBIN - 1] *= std::exp(-(QBIN / PARJ[92]) * (QBIN / PARJ[92]));
        }
        if (IBIN >= 2) BEI[IBIN - 1] += BEI[IBIN - 2];
      }
    }

    // Loop through particle pairs and find old relative momentum.
    for (int I1M = NBE[IBE - 1] + 1; I1M < NBE[IBE]; ++I1M) {
      int I1 = K[0][I1M - 1];
      for (int I2M = I1M + 1; I2M < NBE[IBE] + 1; ++I2M) {
        int I2 = K[0][I2M - 1];
        double Q2OLD =
            std::max(0., (P[3][I1 - 1] + P[3][I2 - 1]) * (P[3][I1 - 1] + P[3][I2 - 1]) -
                             (P[0][I1 - 1] + P[0][I2 - 1]) * (P[0][I1 - 1] + P[0][I2 - 1]) -
                             (P[1][I1 - 1] + P[1][I2 - 1]) * (P[1][I1 - 1] + P[1][I2 - 1]) -
                             (P[2][I1 - 1] + P[2][I2 - 1]) * (P[2][I1 - 1] + P[2][I2 - 1]) -
                             (P[4][I1 - 1] + P[4][I2 - 1]) * (P[4][I1 - 1] + P[4][I2 - 1]));
        double QOLD = std::sqrt(Q2OLD);

        // Calculate new relative momentum.
        double QMOV = 0.;
        if (QOLD < (1e-3 * QDEL)) {
          continue;
        } else if (QOLD <= QDEL) {
          QMOV = QOLD / 3.;
        } else if (QOLD < (NBIN - 0.1) * QDEL) {
          double RBIN = QOLD / QDEL;
          int IBIN = RBIN;
          double RINP = (RBIN * RBIN * RBIN - IBIN * IBIN * IBIN) / (3 * IBIN * (IBIN + 1) + 1);
          QMOV = (BEI[IBIN - 1] + RINP * (BEI[IBIN] - BEI[IBIN - 1])) *
                 std::sqrt(Q2OLD + PMHQ * PMHQ) / Q2OLD;
        } else {
          QMOV = BEI[NBIN - 1] * std::sqrt(Q2OLD + PMHQ * PMHQ) / Q2OLD;
        }
        double Q2NEW = Q2OLD * std::pow(QOLD / (QOLD + 3. * PARJ[91] * QMOV), 2. / 3.);

        // Calculate and save shift to be performed on three-momenta.
        double HC1 =
            (P[3][I1 - 1] + P[3][I2 - 1]) * (P[3][I1 - 1] + P[3][I2 - 1]) - (Q2OLD - Q2NEW);
        double HC2 =
            (Q2OLD - Q2NEW) * (P[3][I1 - 1] - P[3][I2 - 1]) * (P[3][I1 - 1] - P[3][I2 - 1]);
        double HA = 0.5 * (1. - std::sqrt(HC1 * Q2NEW / (HC1 * Q2OLD - HC2)));
        for (int J = 0; J < 3; ++J) {
          double PD = HA * (P[J - 1][I2 - 1] - P[J - 1][I1 - 1]);
          P[J - 1][I1M - 1] += PD;
          P[J - 1][I2M - 1] -= PD;
        }
      }
    }
  }

  // Shift momenta and recalculate energies.
  for (int IM = NBE[0] + 1; IM < NBE[std::min(9, MSTJ[51])] + 1; ++IM) {
    int I = K[0][IM - 1];
    for (int J = 0; J < 3; ++J) {
      P[J][I - 1] += P[J][IM - 1];
    }
    P[3][I - 1] = std::sqrt(P[4][I - 1] * P[4][I - 1] + P[0][I - 1] * P[0][I - 1] +
                            P[1][I - 1] * P[1][I - 1] + P[2][I - 1] * P[2][I - 1]);
  }

  // Boost back to correct reference frame.
  LUDBRB(0, 0, 0., 0., DPS[0] / DPS[3], DPS[1] / DPS[3], DPS[2] / DPS[3]);
  for (int I = 0; I < N; ++I) {
    if (K[0][I] < 0) K[0][I] = -K[0][I];
  }
  return;
}

void sophia_interface::LUJOIN(int NJOIN, int IJOIN[]) {
  // Purpose: to connect a sequence of partons with colour flow indices,
  // as required for subsequent shower evolution (or other operations).
  // Check that partons are of right types to be connected.
  if (NJOIN < 2) {
    // Error exit: no action taken.
    LUERRM(12, "LUJOIN (1): given entries can not be joined by one string");
    return;
  }
  int KQSUM = 0;
  int KQS = 0;
  bool raiseError = false;
  for (int IJN = 1; IJN < NJOIN + 1; ++IJN) {
    int I = IJOIN[IJN - 1];
    if (I <= 0 || I > N) {
      raiseError = true;
      break;
    }
    if (K[0][I - 1] < 1 || K[0][I - 1] > 3) break;
    int KC = LUCOMP(K[1][I - 1]);
    if (KC == 0) {
      raiseError = true;
      break;
    }
    int sgn = (K[1][I - 1] < 0) ? -1 : 1;
    int KQ = KCHG[1][KC - 1] * sgn;
    if (KQ == 0) {
      raiseError = true;
      break;
    }
    if (IJN != 1 && IJN != NJOIN && KQ != 2) {
      raiseError = true;
      break;
    }
    if (KQ != 2) KQSUM += KQ;
    if (IJN == 1) KQS = KQ;
  }
  if (KQSUM != 0) raiseError = true;
  if (raiseError) {
    // Error exit: no action taken.
    LUERRM(12, "LUJOIN (2): given entries can not be joined by one string");
    return;
  }

  // Connect the partons sequentially (closing for gluon loop).
  int KCS = (9 - KQS) / 2;
  if (KQS == 2) KCS = static_cast<int>(4.5 + RLU());
  for (int IJN = 1; IJN < NJOIN + 1; ++IJN) {
    int I = IJOIN[IJN - 1];
    K[0][I - 1] = 3;
    int IP = 0;
    int IN = 0;
    if (IJN != 1) IP = IJOIN[IJN - 2];
    if (IJN == 1) IP = IJOIN[NJOIN - 1];
    if (IJN != NJOIN) IN = IJOIN[IJN];
    if (IJN == NJOIN) IN = IJOIN[0];
    K[KCS - 1][I - 1] = MSTU[4] * IN;
    K[9 - KCS - 1][I - 1] = MSTU[4] * IP;
    if (IJN == 1 && KQS != 2) K[9 - KCS - 1][I - 1] = 0;
    if (IJN == NJOIN && KQS != 2) K[KCS - 1][I - 1] = 0;
  }
  return;
}

void sophia_interface::LUERRM(int MERR, std::string CHMESS) {
  // Purpose: to inform user of errors in program execution.
  // VERY IMPORTANT NOTE: in the original version, all error messages have been set
  // to silently fail!!! This option was set with MSTU[20] = 0. While operating SOPHIA,
  // silent fails actually happend now and then!

  // Write first few warnings, then be silent.
  if (MERR <= 10) {
    MSTU[26]++;
    MSTU[27] = MERR;
    if (MSTU[24] == 1 && MSTU[26] <= MSTU[25])
      std::cout << "LUERRM (1): " << MSTU[10] << " " << MERR << " " << MSTU[30] << " " << CHMESS
                << std::endl;

    // Write first few errors, then be silent or stop program.
  } else if (MERR <= 20) {
    MSTU[22]++;
    MSTU[23] = MERR - 10;
    if (MSTU[20] >= 1 && MSTU[22] <= MSTU[21])
      std::cout << "LUERRM (2): " << MSTU[10] << " " << MERR - 10 << " " << MSTU[30] << " "
                << CHMESS << std::endl;
    if (MSTU[20] >= 2 && MSTU[22] > MSTU[21]) {
      std::cout << "LUERRM (3): " << MSTU[10] << " " << MERR - 10 << " " << MSTU[30] << " "
                << CHMESS << std::endl;
      throw std::runtime_error("LUERRM: critical error; shutting down.");
    }

    // Stop program in case of irreparable error.
  } else {
    std::cout << MSTU[10] << " " << MERR - 20 << " " << MSTU[30] << " " << CHMESS << std::endl;
    throw std::runtime_error("LUERRM: irreparable error.");
  }

  return;
}

void sophia_interface::LUDBRB(int IMIN, int IMAX, double THE, double PHI, double DBX, double DBY,
                              double DBZ, bool skip) {
  double ROT[3][3] = {0.};
  double PR[3] = {0.};
  double VR[3] = {0.};
  double DP[3] = {0.};
  double DV[4] = {0.};
  if (!skip) {
    if (IMIN <= 0) IMIN = 1;
    if (IMAX <= 0) IMAX = N;

    // Optional resetting of V (when not set before.)
    if (MSTU[32] != 0) {
      for (int I = std::min(IMIN, MSTU[3]); I < std::min(IMAX, MSTU[3]) + 1; ++I) {
        for (int J = 0; J < 5; ++J) {
          V[J][I - 1] = 0.;
        }
      }
      MSTU[32] = 0;
    }
  }

  // Check range of rotation/boost.
  if (IMIN > MSTU[3] || IMAX > MSTU[3]) {
    LUERRM(11, "LUDBRB: range outside LUJETS memory");
    return;
  }

  // Rotate, typically from z axis to direction (theta,phi).
  if (THE * THE + PHI * PHI < 1e-20) {
    ROT[0][0] = std::cos(THE) * std::cos(PHI);
    ROT[1][0] = -std::sin(PHI);
    ROT[2][0] = std::sin(THE) * std::cos(PHI);
    ROT[0][1] = std::cos(THE) * std::sin(PHI);
    ROT[1][1] = std::cos(PHI);
    ROT[2][1] = std::sin(THE) * std::sin(PHI);
    ROT[0][2] = -std::sin(THE);
    ROT[1][2] = 0.;
    ROT[2][2] = std::cos(THE);
    for (int I = IMIN; I < IMAX + 1; ++I) {
      if (K[0][I - 1] <= 0) continue;
      for (int J = 0; J < 3; ++J) {
        PR[J] = P[J][I - 1];
        VR[J] = V[J][I - 1];
      }
      for (int J = 0; J < 3; ++J) {
        P[J][I - 1] = ROT[0][J] * PR[0] + ROT[1][J] * PR[1] + ROT[2][J] * PR[2];
        V[J][I - 1] = ROT[0][J] * VR[0] + ROT[1][J] * VR[1] + ROT[2][J] * VR[2];
      }
    }
  }

  // debug("LUDBRB", false);
  // Boost, typically from rest to momentum/energy=beta.
  if (DBX * DBX + DBY * DBY + DBZ * DBZ > 1e-20) {
    double DB = std::sqrt(DBX * DBX + DBY * DBY + DBZ * DBZ);
    if (DB > 0.99999999) {
      LUERRM(3, "(LUROBO:) boost vector too large");
      DBX *= (0.99999999 / DB);
      DBY *= (0.99999999 / DB);
      DBZ *= (0.99999999 / DB);
      DB = 0.99999999;
    }
    double DGA = 1. / std::sqrt(1. - DB * DB);
    for (int I = IMIN; I < IMAX + 1; ++I) {
      if (K[0][I - 1] <= 0) continue;
      for (int J = 0; J < 4; ++J) {
        DP[J] = P[J][I - 1];
        DV[J] = V[J][I - 1];
      }
      double DBP = DBX * DP[0] + DBY * DP[1] + DBZ * DP[2];
      double DGABP = DGA * (DGA * DBP / (1. + DGA) + DP[3]);
      P[0][I - 1] = DP[0] + DGABP * DBX;
      P[1][I - 1] = DP[1] + DGABP * DBY;
      P[2][I - 1] = DP[2] + DGABP * DBZ;
      P[3][I - 1] = DGA * (DP[3] + DBP);
      double DBV = DBX * DV[0] + DBY * DV[1] + DBZ * DV[2];
      double DGABV = DGA * (DGA * DBV / (1. + DGA) + DV[3]);
      V[0][I - 1] = DV[0] + DGABV * DBX;
      V[1][I - 1] = DV[1] + DGABV * DBY;
      V[2][I - 1] = DV[2] + DGABV * DBZ;
      V[3][I - 1] = DGA * (DV[3] + DBV);
    }
  }
  return;
}

void sophia_interface::LUROBO(double THE, double PHI, double BEX, double BEY, double BEZ) {
  // Purpose: to perform rotations and boosts.
  // Find range of rotation/boost. Convert boost to double precision.
  int IMIN = 1;
  if (MSTU[0] < 0) IMIN = MSTU[0];
  int IMAX = N;
  if (MSTU[1] < 0) IMAX = MSTU[1];
  bool skip = true;
  LUDBRB(IMIN, IMAX, THE, PHI, BEX, BEY, BEZ, skip);
}

int sophia_interface::KLU(int I, int J) {
  // Purpose: to provide various integer-valued event related data.
  // Default value. For I=0 number of entries, number of stable entries
  // or 3 times total charge.
  int result = 0;
  int KFA = 0;
  int KC = 0;
  if (I < 0 || I > MSTU[3] || J <= 0) {
    // do nothing
  } else if (I == 0 && J == 1) {
    result = N;
  } else if (I == 0 && (J == 2 || J == 6)) {
    for (int I1 = 0; I1 < N; ++I1) {
      if (J == 2 && K[0][I1] >= 1 && K[0][I1] <= 10) result++;
      if (J == 6 && K[0][I1] >= 1 && K[0][I1] <= 10) result += LUCHGE(K[1][I1]);
    }
  } else if (I == 0) {
    // do nothing
    // For I > 0 direct readout of K matrix or charge.
  } else if (J <= 5) {
    result = K[J - 1][I - 1];
  } else if (J == 6) {
    result = LUCHGE(K[1][I - 1]);

    // Status (existing/fragmented/decayed), parton/hadron separation.
  } else if (J <= 8) {
    if (K[0][I - 1] >= 1 && K[0][I - 1] <= 10) result = 1;
    if (J == 8) result *= K[1][I - 1];
  } else if (J <= 12) {
    KFA = std::abs(K[1][I - 1]);
    KC = LUCOMP(KFA);
    int KQ = 0;
    if (KC != 0) KQ = KCHG[1][KC - 1];
    if (J == 9 && KC != 0 && KQ != 0) result = K[1][I - 1];
    if (J == 10 && KC != 0 && KQ == 0) result = K[1][I - 1];
    if (J == 11) result = KC;
    if (J == 12) {
      int sgn = (K[1][I - 1] < 0) ? -1 : 1;
      result = KQ * sgn;
    }

    // Heaviest flavour in hadron/diquark.
  } else if (J == 13) {
    KFA = std::abs(K[1][I - 1]);
    result = (KFA / 100) % 10 * std::pow(-1, (KFA / 100) % 10);
    if (KFA < 10) result = KFA;
    if ((KFA / 1000) % 10 != 0) result = (KFA / 1000) % 10;
    int sgn = (K[1][I - 1] < 0) ? -1 : 1;
    result = result * sgn;

    // Particle history: generation, ancestor, rank.
  } else if (J <= 15) {
    int I2 = I;
    int I1 = I;
    do {
      result++;
      I2 = I1;
      I1 = K[2][I1 - 1];
    } while (I1 > 0 && K[0][I1 - 1] > 0 && K[0][I1 - 1] <= 20);
    if (J == 15) result = I2;
  } else if (J == 16) {
    KFA = std::abs(K[1][I - 1]);
    if (K[0][I - 1] <= 20 &&
        ((KFA >= 11 && KFA <= 20) || KFA == 22 || (KFA > 100 && (KFA / 10) % 10 != 0))) {
      int I1 = I;
      int ILP = 1;
      int I2 = I1;
      do {
        I2 = I1;
        I1 = K[2][I1 - 1];
        if (I1 > 0) {
          int KFAM = std::abs(K[1][I1 - 1]);
          if (KFAM = !0 && KFAM <= 10) ILP = 0;
          if (KFAM == 21 || KFAM == 91 || KFAM == 92 || KFAM == 93) ILP = 0;
          if (KFAM > 100 && (KFAM / 10) % 10 == 0) ILP = 0;
        }
      } while (ILP == 1 && I1 > 0);

      if (K[0][I1 - 1] == 12) {
        for (int I3 = I1; I3 < I2; ++I3) {
          if (K[2][I3] == K[2][I2 - 1] && K[1][I3] != 91 && K[1][I3] != 92 && K[1][I3] != 93)
            result++;
        }
      } else {
        int I3 = I2;
        do {
          result++;
          I3++;
        } while (I3 < N && K[2][I3 - 1] == K[2][I2 - 1]);
      }
    }

    // Particle coming from collapsing jet system or not.
  } else if (J == 17) {
    int I1 = I;
    int I3 = I1;
    do {
      result++;
      I3 = I1;
      I1 = K[2][I1 - 1];
      int I0 = std::max(1, I1);
      KC = LUCOMP(K[1][I0 - 1]);
      if (I1 == 0 || K[0][I0 - 1] <= 0 || K[0][I0 - 1] > 20 || KC == 0) {
        if (result == 1) result = -1;
        if (result > 1) result = 0;
        return result;
      }
    } while (KCHG[1][KC - 1] == 0);
    if (K[0][I1 - 1] != 12) return 0;
    int I2 = I1;
    do {
      I2++;
    } while (I2 < N && K[0][I2 - 1] != 11);
    int K3M = K[2][I3 - 2];
    if (K3M >= I1 && K3M <= I2) result = 0;
    int K3P = K[2][I3];
    if (I3 < N && K3P >= I1 && K3P <= I2) result = 0;

    // Number of decay products. Colour flow.
  } else if (J == 18) {
    if (K[0][I - 1] == 11 || K[0][I - 1] == 12) result = std::max(0, K[4][I - 1] - K[3][I - 1] + 1);
    if (K[3][I - 1] == 0 || K[4][I - 1] == 0) result = 0;
  } else if (J <= 22) {
    if (K[0][I - 1] != 3 && K[0][I - 1] != 13 && K[0][I - 1] != 14) return result;
    if (J == 19) result = (K[3][I - 1] / MSTU[4]) % MSTU[4];
    if (J == 20) result = (K[4][I - 1] / MSTU[4]) % MSTU[4];
    if (J == 21) result = K[3][I - 1] % MSTU[4];
    if (J == 22) result = K[4][I - 1] % MSTU[4];
  }

  return result;
}

static int MRLU[6] = {19780503, 0, 0, 97, 33, 0};
static double RRLU[100] = {0.};
double sophia_interface::RLU(bool isCalledByRNDM) {
  // Purpose: to generate random numbers uniformly distributed between
  // 0 and 1, excluding the endpoints.
  // Initialize generation from given seed.
  if (MRLU[1] == 0) {
    if (isCalledByRNDM && MRLU[0] == 0) MRLU[0] = 19780503;  // initial seed if called by RNDM
    int IJ = (MRLU[0] / 30082) % 31329;
    int KL = MRLU[0] % 30082;
    int I = (IJ / 177) % 177 + 2;
    int J = IJ % 177 + 2;
    int K = (KL / 169) % 178 + 1;
    int L = KL % 169;
    for (int II = 1; II < 98; ++II) {
      double S = 0.;
      double T = 0.5;
      for (int JJ = 1; JJ < 25; ++JJ) {
        int M = ((I * J) % 179 * K) % 179;
        I = J;
        J = K;
        K = M;
        L = (53 * L + 1) % 169;
        if ((L * M) % 64 >= 32) S += T;
        T *= 0.5;
      }
      RRLU[II - 1] = S;
    }
    double TWOM24 = std::pow(0.5, 24);
    RRLU[97] = 362436. * TWOM24;
    RRLU[98] = 7654321. * TWOM24;
    RRLU[99] = 16777213. * TWOM24;
    MRLU[1] = 1;
    MRLU[2] = 0;
    MRLU[3] = 97;
    MRLU[4] = 33;
  }

  // Generate next random number.
  double RUNI = 0.;  // this is the result of the RNG
  do {
    RUNI = RRLU[MRLU[3] - 1] - RRLU[MRLU[4] - 1];
    if (RUNI < 0.) RUNI++;
    RRLU[MRLU[3] - 1] = RUNI;
    MRLU[3]--;
    if (MRLU[3] == 0) MRLU[3] = 97;
    MRLU[4]--;
    if (MRLU[4] == 0) MRLU[4] = 97;
    RRLU[97] -= RRLU[98];
    if (RRLU[97] < 0.) RRLU[97] += RRLU[99];
    RUNI -= RRLU[97];
    if (RUNI < 0.) RUNI++;
  } while (RUNI <= 0 || RUNI >= 1.);

  // Update counters. Random number to output.
  MRLU[2]++;
  if (MRLU[2] == 1000000000) {
    MRLU[1]++;
    MRLU[2] = 0;
  }
  return RUNI;
}

double sophia_interface::ULMASS(int KF) {
  // Purpose: to give the mass of a particle/parton.
  double result = 0.;
  int KFA = std::abs(KF);
  int KC = LUCOMP(KF);
  if (KC == 0) return 0;
  PARF[105] = PMAS[0][5];
  PARF[106] = PMAS[0][6];
  PARF[107] = PMAS[0][7];

  // Guarantee use of constituent masses for internal checks.
  if ((MSTJ[92] == 1 || MSTJ[92] == 2) && KFA <= 10) {
    result = PARF[99 + KFA];
    if (MSTJ[92] == 2) result = std::max(0., result - PARF[120]);

    // Masses that can be read directly off table.
  } else if (KFA <= 100 || KC <= 80 || KC > 100) {
    result = PMAS[0][KC - 1];

    // Find constituent partons and their masses.
  } else {
    int KFLA = (KFA / 1000) % 10;
    int KFLB = (KFA / 100) % 10;
    int KFLC = (KFA / 10) % 10;
    int KFLS = KFA % 10;
    int KFLR = (KFA / 10000) % 10;
    double PMA = PARF[99 + KFLA];
    double PMB = PARF[99 + KFLB];
    double PMC = PARF[99 + KFLC];
    double PMSPL = 0.;

    // Construct masses for various meson, diquark and baryon cases.
    if (KFLA == 0 && KFLR == 0 && KFLS <= 3) {
      if (KFLS == 1) PMSPL = -3. / (PMB * PMC);
      if (KFLS >= 3) PMSPL = 1. / (PMB * PMC);
      result = PARF[110] + PMB + PMC + PARF[112] * PARF[100] * PARF[100] * PMSPL;
    } else if (KFLA == 0) {
      int KMUL = 2;
      if (KFLS == 1) KMUL = 3;
      if (KFLR == 2) KMUL = 4;
      if (KFLS == 5) KMUL = 5;
      result = PARF[112 + KMUL] + PMB + PMC;
    } else if (KFLC == 0) {
      if (KFLS == 1) PMSPL = -3. / (PMA * PMB);
      if (KFLS == 3) PMSPL = 1. / (PMA * PMB);
      result = 2. * PARF[111] / 3. + PMA + PMB + PARF[113] * PARF[100] * PARF[100] * PMSPL;
      if (MSTJ[92] == 1) result = PMA + PMB;
      if (MSTJ[92] == 2) result = std::max(0., result - PARF[121] - 2. * PARF[111] / 3.);
    } else {
      if (KFLS == 2 && KFLA == KFLB) {
        PMSPL = 1. / (PMA * PMB) - 2. / (PMA * PMC) - 2. / (PMB * PMC);
      } else if (KFLS == 2 && KFLB >= KFLC) {
        PMSPL = -2. / (PMA * PMB) - 2. / (PMA * PMC) + 1. / (PMB * PMC);
      } else if (KFLS == 2) {
        PMSPL = -3. / (PMB * PMC);
      } else {
        PMSPL = 1. / (PMA * PMB) + 1. / (PMA * PMC) + 1. / (PMB * PMC);
      }
      result = PARF[111] + PMA + PMB + PMC + PARF[113] * PARF[100] * PARF[100] * PMSPL;
    }
  }

  // Optional mass broadening according to truncated Breit-Wigner (either in m or in m^2).
  if (MSTJ[23] >= 1 && PMAS[1][KC - 1] > 1e-4) {
    if (MSTJ[23] == 1 || (MSTJ[23] == 2 && KFA > 100)) {
      result += 0.5 * PMAS[1][KC - 1] *
                std::tan((2. * RLU() - 1.) * std::atan(2. * PMAS[2][KC - 1] / PMAS[1][KC - 1]));
    } else {
      double PM0 = result;
      double PMLOW = std::atan((std::pow(std::max(0., PM0 - PMAS[2][KC - 1]), 2) - PM0 * PM0) /
                               (PM0 * PMAS[1][KC - 1]));
      double PMUPP =
          std::atan((std::pow((PM0 + PMAS[2][KC - 1]), 2) - PM0 * PM0) / (PM0 * PMAS[1][KC - 1]));
      result = std::sqrt(std::max(
          0., PM0 * PM0 + PM0 * PMAS[1][KC - 1] * std::tan(PMLOW + (PMUPP - PMLOW) * RLU())));
    }
  }
  MSTJ[92] = 0;
  return result;
}

double sophia_interface::ULANGL(double X, double Y) {
  // Purpose: to reconstruct an angle from given x and y coordinates.
  double result = 0.;
  double R = std::sqrt(X * X + Y * Y);
  if (R < 1e-20) return 0.;
  if (std::abs(X) / R < 0.8) {
    int sgn = (Y < 0) ? -1 : 1;
    result = std::acos(X / R) * sgn;
  } else {
    result = std::asin(Y / R);
    if (X < 0. && result >= 0.) {
      result = PARU[0] - result;
    } else if (X < 0.) {
      result = -PARU[0] - result;
    }
  }
  return result;
}

double sophia_interface::PLU(int I, int J) {
  // Purpose: to provide various real-valued event related data.
  double PSUM[4];

  // Set default value. For I = 0 sum of momenta or charges, or invariant mass of system.
  double result = 0.;
  if (I < 0 || I > MSTU[3] || J <= 0) {
    // do nothing
  } else if (I == 0 && J <= 4) {
    for (int I1 = 0; I1 < N; ++I1) {
      if (K[0][I1] > 0 && K[0][I1] <= 10) {
        result += P[J - 1][I1];
      }
    }
  } else if (I == 0 && J == 5) {
    for (int J1 = 0; J1 < 4; ++J1) {
      PSUM[J1] = 0.;
      for (int I1 = 0; I1 < N; ++I1) {
        if (K[0][I1] > 0 && K[0][I1] <= 10) {
          PSUM[J1] += P[J1][I1];
        }
      }
    }
    result = std::sqrt(std::max(
        0., PSUM[3] * PSUM[3] - PSUM[0] * PSUM[0] - PSUM[1] * PSUM[1] - PSUM[2] * PSUM[2]));
  } else if (I == 0 && J == 6) {
    for (int I1 = 0; I1 < N; ++I1) {
      if (K[0][I1] > 0 && K[0][I1] <= 10) {
        result += LUCHGE(K[1][I1]) / 3.;
      }
    }
  } else if (I == 0) {
    // do nothing

    // Direct readout of P matrix.
  } else if (J <= 15) {
    result = P[J - 1][I - 1];

    // Charge, total momentum, transverse momentum, transverse mass.
  } else if (J <= 12) {
    if (J == 6) result = LUCHGE(K[1][I - 1]) / 3.;
    if (J == 7 || J == 8)
      result = P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1] + P[2][I - 1] * P[2][I - 1];
    if (J == 9 || J == 10) result = P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1];
    if (J == 11 || J == 12)
      result = P[4][I - 1] * P[4][I - 1] + P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1];
    if (J == 8 || J == 10 || J == 12) result = std::sqrt(result);

    // Theta and phi angle in radians or degrees.
  } else if (J <= 16) {
    if (J <= 14)
      result =
          ULANGL(P[2][I - 1], std::sqrt(P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1]));
    if (J >= 14) result = ULANGL(P[0][I - 1], P[1][I - 1]);
    if (J == 14 || J == 16) result *= 180. / PARU[0];

    // True rapidity, rapidity with pion mass, pseudorapidity.
  } else if (J <= 19) {
    double PMR = 0.;
    if (J == 17) PMR = P[4][I - 1];
    if (J == 18) PMR = ULMASS(211);
    double PR = std::max(1e-20, PMR * PMR + P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1]);
    result = std::log(std::min(
        (std::sqrt(PR + P[2][I - 1] * P[2][I - 1]) + std::abs(P[2][I - 1])) / std::sqrt(PR), 1e20));
    int sgn = (P[2][I - 1] < 0) ? -1 : 1;
    result *= sgn;

    // Energy and momentum fractions (only to be used in CM frame).
  } else if (J <= 25) {
    if (J == 20)
      result = 2. *
               std::sqrt(P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1] +
                         P[2][I - 1] * P[2][I - 1]) /
               PARU[20];
    if (J == 21) result = 2. * P[2][I - 1] / PARU[20];
    if (J == 22)
      result = 2. * std::sqrt(P[0][I - 1] * P[0][I - 1] + P[1][I - 1] * P[1][I - 1]) / PARU[20];
    if (J == 23) result = 2. * P[3][I - 1] / PARU[20];
    if (J == 24) result = (P[3][I - 1] + P[2][I - 1]) / PARU[20];
    if (J == 25) result = (P[3][I - 1] - P[2][I - 1]) / PARU[20];
  }
  return result;
}

int sophia_interface::LUCHGE(int KF) {
  // Purpose: to give three times the charge for a particle/parton.
  // Initial values. Simple case of direct readout.
  int result = 0;
  int KFA = std::abs(KF);
  int KC = LUCOMP(KFA);
  if (KC == 0) {
    // do nothing
  } else if (KFA <= 100 || KC <= 80 || KC > 100) {
    result = KCHG[0][KC - 1];

    // Construction from quark content for heavy meson, diquark, baryon.
  } else if ((KFA / 1000) % 10 == 0) {
    result = (KCHG[0][((KFA / 100) % 10) - 1] - KCHG[0][((KFA / 10) % 10) - 1]) *
             std::pow(-1, (KFA / 100) % 10);
  } else if ((KFA / 10) % 10 == 0) {
    result = KCHG[0][((KFA / 1000) % 10) - 1] + KCHG[0][((KFA / 100) % 10) - 1];
  } else {
    result = KCHG[0][((KFA / 1000) % 10) - 1] + KCHG[0][((KFA / 100) % 10) - 1] +
             KCHG[0][((KFA / 10) % 10) - 1];
  }

  // Add on correct sign.
  int sgn = (KF < 0) ? -1 : 1;
  return result * sgn;
}

int sophia_interface::LUCOMP(int KF) {
  // Purpose: to compress the standard KF codes for use in mass and decay
  // arrays; also to check whether a given code actually is defined.
  static int KFTAB[25] = {211,  111, 221,  311,  321, 130, 310, 213, 113,   223,   313, 323, 2112,
                          2212, 210, 2110, 2210, 110, 220, 330, 440, 30443, 30553, 0,   0};
  static int KCTAB[25] = {101, 111, 112, 102, 103, 221, 222, 121, 131, 132, 122, 123, 332,
                          333, 281, 282, 283, 284, 285, 286, 287, 231, 235, 0,   0};
  int result = 0;
  int KFA = std::abs(KF);

  // Simple cases: direct translation or table.
  if (KFA == 0 || KFA >= 100000) {
    return 0;
  } else if (KFA <= 100) {
    result = KFA;
    if (KF < 0 && KCHG[2, KFA - 1] == 0) return 0;
  } else {
    for (int IKF = 0; IKF < 23; ++IKF) {
      if (KFA == KFTAB[IKF]) {
        result = KCTAB[IKF];
        if (KF < 0 && KCHG[2, result - 1] == 0) return 0;
      }
    }
  }

  // Subdivide KF code into constituent pieces.
  int KFLA = (KFA / 1000) % 10;
  int KFLB = (KFA / 100) % 10;
  int KFLC = (KFA / 10) % 10;
  int KFLS = KFA % 10;
  int KFLR = (KFA / 10000) % 10;

  // Mesons.
  if (KFA - 10000 * KFLR < 1000) {
    if (KFLB == 0 || KFLB == 9 || KFLC == 0 || KFLC == 9) {
      // do nothing
    } else if (KFLB < KFLC) {
      // do nothing
    } else if (KF < 0 && KFLB == KFLC) {
      // do nothing
    } else if (KFLB == KFLC) {
      if (KFLR == 0 && KFLS == 1) {
        result = 110 + KFLB;
      } else if (KFLR == 0 && KFLS == 3) {
        result = 130 + KFLB;
      } else if (KFLR == 1 && KFLS == 3) {
        result = 150 + KFLB;
      } else if (KFLR == 1 && KFLS == 1) {
        result = 170 + KFLB;
      } else if (KFLR == 2 && KFLS == 3) {
        result = 190 + KFLB;
      } else if (KFLR == 0 && KFLS == 5) {
        result = 210 + KFLB;
      }
    } else if (KFLB <= 5) {
      if (KFLR == 0 && KFLS == 1) {
        result = 100 + ((KFLB - 1) * (KFLB - 2)) / 2 + KFLC;
      } else if (KFLR == 0 && KFLS == 3) {
        result = 120 + ((KFLB - 1) * (KFLB - 2)) / 2 + KFLC;
      } else if (KFLR == 1 && KFLS == 3) {
        result = 140 + ((KFLB - 1) * (KFLB - 2)) / 2 + KFLC;
      } else if (KFLR == 1 && KFLS == 1) {
        result = 160 + ((KFLB - 1) * (KFLB - 2)) / 2 + KFLC;
      } else if (KFLR == 2 && KFLS == 3) {
        result = 180 + ((KFLB - 1) * (KFLB - 2)) / 2 + KFLC;
      } else if (KFLR == 0 && KFLS == 5) {
        result = 200 + ((KFLB - 1) * (KFLB - 2)) / 2 + KFLC;
      }
    } else if ((KFLS == 1 && KFLR <= 1) || (KFLS == 3 && KFLR <= 2) || (KFLS == 5 && KFLR == 0)) {
      result = 80 + KFLB;
    }

    // Diquarks.
  } else if ((KFLR == 0 || KFLR == 1) && KFLC == 0) {
    if (KFLS != 1 && KFLS != 3) {
      // do nothing
    } else if (KFLA == 9 || KFLB == 0 || KFLB == 9) {
      // do nothing
    } else if (KFLA < KFLB) {
      // do nothing
    } else if (KFLS == 1 && KFLA == KFLB) {
      // do nothing
    } else {
      result = 90;
    }

    // Spin 1/2 baryons.
  } else if (KFLR == 0 && KFLS == 2) {
    if (KFLA == 9 || KFLB == 0 || KFLB == 9 || KFLC == 9) {
      // do nothing
    } else if (KFLA <= KFLC || KFLA < KFLB) {
      // do nothing
    } else if (KFLA >= 6 || KFLB >= 4 || KFLC >= 4) {
      result = 80 + KFLA;
    } else if (KFLB < KFLC) {
      result = 300 + ((KFLA + 1) * KFLA * (KFLA - 1)) / 6 + (KFLC * (KFLC - 1)) / 2 + KFLB;
    } else {
      result = 330 + ((KFLA + 1) * KFLA * (KFLA - 1)) / 6 + (KFLB * (KFLB - 1)) / 2 + KFLC;
    }

    // Spin 3/2 baryons.
  } else if (KFLR == 0 && KFLS == 4) {
    if (KFLA == 9 || KFLB == 0 || KFLB == 9 || KFLC == 9) {
      // do nothing
    } else if (KFLA < KFLB || KFLB < KFLC) {
      // do nothing
    } else if (KFLA >= 6 || KFLB >= 4) {
      result = 80 + KFLA;
    } else {
      result = 360 + ((KFLA + 1) * KFLA * (KFLA - 1)) / 6 + (KFLB * (KFLB - 1)) / 2 + KFLC;
    }
  }

  return result;
}

int ID_sophia_to_PDG(int sophiaID) {
  double id = 0.;
  switch (sophiaID) {
    case 13:  // proton
      id = 1000010010;
      break;
    case 14:  // neutron
      id = 1000000010;
      break;
    case -13:  // anti-proton
      id = -1000010010;
      break;
    case -14:  // anti-neutron
      id = -1000000010;
      break;
    case 1:  // photon
      id = 22;
      break;
    case 2:  // positron
      id = -11;
      break;
    case 3:  // electron
      id = 11;
      break;
    case 15:  // nu_e
      id = 12;
      break;
    case 16:  // anti-nu_e
      id = -12;
      break;
    case 17:  // nu_mu
      id = 14;
      break;
    case 18:  // anti-nu_mu
      id = -14;
      break;
    case 6:  // pi0
      id = 111;
      break;
    case 7:  // pi+
      id = 211;
      break;
    case 8:  // -211
      id = -211;
      break;
    default:
      throw std::runtime_error("ID_sophia_to_PDG: unkown particle ID");
  }
  return id;
}

// prototype for qq interactions. This is a separate entry to JETSET
void sophia_interface::LU2ENT(int KF1, int KF2, double PECM) {
  // Purpose: to store two partons/particles in their CM frame, with the first along the +z axis.

  // Standard checks.
  MSTU[27] = 0;
  int IPA = 1;
  if (IPA > MSTU[3] - 1) LUERRM(21, "(LU2ENT:) writing outside LUJETS memory");
  int KC1 = LUCOMP(KF1);
  int KC2 = LUCOMP(KF2);
  if (KC1 == 0 || KC2 == 0) LUERRM(12, "(LU2ENT:) unknown flavour code");

  // Find masses. Reset K, P and V vectors.
  double PM1 = 0.;
  if (MSTU[9] == 1) PM1 = P[4][IPA - 1];
  if (MSTU[9] >= 2) PM1 = ULMASS(KF1);
  double PM2 = 0.;
  if (MSTU[9] == 1) PM2 = P[4][IPA];
  if (MSTU[9] >= 2) PM2 = ULMASS(KF2);
  for (int i = IPA; i < IPA + 1; ++i) {
    for (int j = 0; j < 5; ++j) {
      K[j][i - 1] = 0;
      P[j][i - 1] = 0.;
      V[j][i - 1] = 0.;
    }
  }

  // Check flavours.
  int sgn1 = (KF1 < 0) ? -1 : 1;
  int KQ1 = KCHG[1][KC1 - 1] * sgn1;
  int sgn2 = (KF2 < 0) ? -1 : 1;
  int KQ2 = KCHG[1][KC2 - 1] * sgn2;
  if (MSTU[18] == 1) {
    MSTU[18] = 0;
  } else {
    if (KQ1 + KQ2 != 0 && KQ1 + KQ2 != 4) LUERRM(2, "(LU2ENT:) unphysical flavour combination");
  }
  K[1][IPA - 1] = KF1;
  K[1][IPA] = KF2;

  // Store partons/particles in K vectors for normal case. (assumes IP = 0)
  K[0][IPA - 1] = 1;
  if (KQ1 != 0 && KQ2 != 0) K[0][IPA - 1] = 2;
  K[0][IPA] = 1;

  // Check kinematics and store partons/particles in P vectors.
  if (PECM <= PM1 + PM2) throw std::runtime_error("(LU2ENT:) CM energy smaller than sum of masses");
  double PA = std::sqrt(std::max(0., std::pow(PECM * PECM - PM1 * PM1 - PM2 * PM2, 2) -
                                         std::pow(2. * PM1 * PM2, 2))) /
              (2. * PECM);
  P[2][IPA - 1] = PA;
  P[3][IPA - 1] = std::sqrt(PM1 * PM1 + PA * PA);
  P[4][IPA - 1] = PM1;
  P[2][IPA] = -PA;
  P[3][IPA] = std::sqrt(PM2 * PM2 + PA * PA);
  P[4][IPA] = PM2;

  // Set N and fragment/decay.
  N = IPA + 1;
  LUEXEC();

  return;
}
