/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/*
 * reeds_shepp_path.cc
 */

#include "modules/planning/open_space/reeds_shepp_path.h"

namespace apollo {
namespace planning {

ReedShepp::ReedShepp(const common::VehicleParam& vehicle_param,
                     const PlannerOpenSpaceConfig& open_space_conf)
    : vehicle_param_(vehicle_param), open_space_conf_(open_space_conf) {
  max_kappa_ = std::tan(open_space_conf.max_steering()) /
               vehicle_param_.front_edge_to_center();
};

std::pair<double, double> ReedShepp::calc_tau_omega(double u, double v,
                                                    double xi, double eta,
                                                    double phi) {
  double delta = common::math::NormalizeAngle(u - v);
  double A = std::sin(u) - std::sin(delta);
  double B = std::cos(u) - std::cos(delta) - 1.0;

  double t1 = std::atan2(eta * A - xi * B, xi * A + eta * B);
  double t2 = 2.0 * (std::cos(delta) - std::cos(v) - std::cos(u)) + 3.0;
  double tau = 0.0;
  if (t2 < 0) {
    tau = common::math::NormalizeAngle(t1 + M_PI);
  } else {
    tau = common::math::NormalizeAngle(t1);
  }
  double omega = common::math::NormalizeAngle(tau - u + v - phi);
  return std::make_pair(tau, omega);
}

bool ReedShepp::ShortestRSP(const std::shared_ptr<Node3d> start_node,
                            const std::shared_ptr<Node3d> end_node,
                            ReedSheppPath* const optimal_path) {
  std::vector<ReedSheppPath> all_possible_paths;
  if (!GenerateRSPs(start_node, end_node, &all_possible_paths)) {
    AINFO << "Fail to generate different combination of Reed Shepp "
             "paths";
    return false;
  }
  double optimal_path_length = std::numeric_limits<double>::infinity();
  int optimal_path_index = -1;
  for (std::size_t i = 0; i < all_possible_paths.size(); i++) {
    if (all_possible_paths.at(i).total_length < optimal_path_length) {
      optimal_path_index = i;
      optimal_path_length = all_possible_paths.at(i).total_length;
    }
  }
  *optimal_path = all_possible_paths.at(optimal_path_index);
  return true;
}

bool ReedShepp::GenerateRSPs(const std::shared_ptr<Node3d> start_node,
                             const std::shared_ptr<Node3d> end_node,
                             std::vector<ReedSheppPath>* all_possible_paths) {
  if (!GenerateRSP(start_node, end_node, all_possible_paths)) {
    AINFO << "Fail to generate general profile of different RSPs";
    return false;
  }
  if (!GenerateLocalConfigurations(start_node, end_node, all_possible_paths)) {
    AINFO << "Fail to generate local configurations(x, y, phi) in RSP";
    return false;
  }
  return true;
}

bool ReedShepp::GenerateRSP(const std::shared_ptr<Node3d> start_node,
                            const std::shared_ptr<Node3d> end_node,
                            std::vector<ReedSheppPath>* all_possible_paths) {
  double dx = end_node->GetX() - start_node->GetX();
  double dy = end_node->GetY() - start_node->GetY();
  double dphi = end_node->GetPhi() - start_node->GetPhi();
  double c = std::cos(start_node->GetPhi());
  double s = std::sin(start_node->GetPhi());
  // normalize the initial point to (0,0,0)
  double x = (c * dx + s * dy) * max_kappa_;
  double y = (-s * dx + c * dy) * max_kappa_;
  if (!SCS(x, y, dphi, all_possible_paths)) {
    AINFO << "Fail at SCS";
    return false;
  }
  if (!CSC(x, y, dphi, all_possible_paths)) {
    AINFO << "Fail at CSC";
    return false;
  }
  if (!CCC(x, y, dphi, all_possible_paths)) {
    AINFO << "Fail at CCC";
    return false;
  }
  if (!CCCC(x, y, dphi, all_possible_paths)) {
    AINFO << "Fail at CCCC";
    return false;
  }
  if (!CCSC(x, y, dphi, all_possible_paths)) {
    AINFO << "Fail at CCSC";
    return false;
  }
  if (!CCSCC(x, y, dphi, all_possible_paths)) {
    AINFO << "Fail at CCSCC";
    return false;
  }
  if (all_possible_paths->size() == 0) {
    return false;
  }
  return true;
}

bool ReedShepp::SCS(double x, double y, double phi,
                    std::vector<ReedSheppPath>* all_possible_paths) {
  RSPParam SLS_param;
  SLS(x, y, phi, &SLS_param);
  double SLS_lengths[3] = {SLS_param.t, SLS_param.u, SLS_param.v};
  char SLS_types[] = "SLS";
  if (SLS_param.flag && !SetRSP(SLS_lengths, SLS_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with SLS_param";
    return false;
  }
  RSPParam SRS_param;
  SLS(x, -y, -phi, &SRS_param);
  double SRS_lengths[3] = {SRS_param.t, SRS_param.u, SRS_param.v};
  char SRS_types[] = "SRS";
  if (SRS_param.flag && !SetRSP(SRS_lengths, SRS_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with SRS_param";
    return false;
  }
  return true;
}

bool ReedShepp::CSC(double x, double y, double phi,
                    std::vector<ReedSheppPath>* all_possible_paths) {
  RSPParam LSL1_param;
  LSL(x, y, phi, &LSL1_param);
  double LSL1_lengths[3] = {LSL1_param.t, LSL1_param.u, LSL1_param.v};
  char LSL1_types[] = "LSL";
  if (LSL1_param.flag &&
      !SetRSP(LSL1_lengths, LSL1_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LSL_param";
    return false;
  }

  RSPParam LSL2_param;
  LSL(-x, y, -phi, &LSL2_param);
  double LSL2_lengths[3] = {-LSL2_param.t, -LSL2_param.u, -LSL2_param.v};
  char LSL2_types[] = "LSL";
  if (LSL2_param.flag &&
      !SetRSP(LSL2_lengths, LSL2_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LSL2_param";
    return false;
  }

  RSPParam LSL3_param;
  LSL(x, -y, -phi, &LSL3_param);
  double LSL3_lengths[3] = {LSL3_param.t, LSL3_param.u, LSL3_param.v};
  char LSL3_types[] = "RSR";
  if (LSL3_param.flag &&
      !SetRSP(LSL3_lengths, LSL3_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LSL3_param";
    return false;
  }

  RSPParam LSL4_param;
  LSL(-x, -y, phi, &LSL4_param);
  double LSL4_lengths[3] = {-LSL4_param.t, -LSL4_param.u, -LSL4_param.v};
  char LSL4_types[] = "RSR";
  if (LSL4_param.flag &&
      !SetRSP(LSL4_lengths, LSL4_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LSL4_param";
    return false;
  }

  RSPParam LSR1_param;
  LSR(x, y, phi, &LSR1_param);
  double LSR1_lengths[3] = {LSR1_param.t, LSR1_param.u, LSR1_param.v};
  char LSR1_types[] = "LSR";
  if (LSR1_param.flag &&
      !SetRSP(LSR1_lengths, LSR1_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LSR1_param";
    return false;
  }

  RSPParam LSR2_param;
  LSR(-x, y, -phi, &LSR2_param);
  double LSR2_lengths[3] = {-LSR2_param.t, -LSR2_param.u, -LSR2_param.v};
  char LSR2_types[] = "LSR";
  if (LSR2_param.flag &&
      !SetRSP(LSR2_lengths, LSR2_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LSR2_param";
    return false;
  }

  RSPParam LSR3_param;
  LSR(x, -y, -phi, &LSR3_param);
  double LSR3_lengths[3] = {LSR3_param.t, LSR3_param.u, LSR3_param.v};
  char LSR3_types[] = "RSL";
  if (LSR3_param.flag &&
      !SetRSP(LSR3_lengths, LSR3_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LSR3_param";
    return false;
  }

  RSPParam LSR4_param;
  LSR(-x, -y, phi, &LSR4_param);
  double LSR4_lengths[3] = {-LSR4_param.t, -LSR4_param.u, -LSR4_param.v};
  char LSR4_types[] = "RSL";
  if (LSR4_param.flag &&
      !SetRSP(LSR4_lengths, LSR4_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LSR4_param";
    return false;
  }
  return true;
}

bool ReedShepp::CCC(double x, double y, double phi,
                    std::vector<ReedSheppPath>* all_possible_paths) {
  RSPParam LRL1_param;
  LRL(x, y, phi, &LRL1_param);
  double LRL1_lengths[3] = {LRL1_param.t, LRL1_param.u, LRL1_param.v};
  char LRL1_types[] = "LRL";
  if (LRL1_param.flag &&
      !SetRSP(LRL1_lengths, LRL1_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRL_param";
    return false;
  }

  RSPParam LRL2_param;
  LRL(-x, y, -phi, &LRL2_param);
  double LRL2_lengths[3] = {-LRL2_param.t, -LRL2_param.u, -LRL2_param.v};
  char LRL2_types[] = "LRL";
  if (LRL2_param.flag &&
      !SetRSP(LRL2_lengths, LRL2_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRL2_param";
    return false;
  }

  RSPParam LRL3_param;
  LRL(x, -y, -phi, &LRL3_param);
  double LRL3_lengths[3] = {LRL3_param.t, LRL3_param.u, LRL3_param.v};
  char LRL3_types[] = "RLR";
  if (LRL3_param.flag &&
      !SetRSP(LRL3_lengths, LRL3_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRL3_param";
    return false;
  }

  RSPParam LRL4_param;
  LRL(-x, -y, phi, &LRL4_param);
  double LRL4_lengths[3] = {-LRL4_param.t, -LRL4_param.u, -LRL4_param.v};
  char LRL4_types[] = "RLR";
  if (LRL4_param.flag &&
      !SetRSP(LRL4_lengths, LRL4_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRL4_param";
    return false;
  }

  // backward
  double xb = x * std::cos(phi) + y * std::sin(phi);
  double yb = x * std::sin(phi) - y * std::cos(phi);

  RSPParam LRL5_param;
  LRL(xb, yb, phi, &LRL5_param);
  double LRL5_lengths[3] = {LRL5_param.v, LRL5_param.u, LRL5_param.t};
  char LRL5_types[] = "LRL";
  if (LRL5_param.flag &&
      !SetRSP(LRL5_lengths, LRL5_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRL5_param";
    return false;
  }

  RSPParam LRL6_param;
  LRL(-xb, yb, -phi, &LRL6_param);
  double LRL6_lengths[3] = {-LRL6_param.v, -LRL6_param.u, -LRL6_param.t};
  char LRL6_types[] = "LRL";
  if (LRL6_param.flag &&
      !SetRSP(LRL6_lengths, LRL6_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRL6_param";
    return false;
  }

  RSPParam LRL7_param;
  LRL(xb, -yb, -phi, &LRL7_param);
  double LRL7_lengths[3] = {LRL7_param.v, LRL7_param.u, LRL7_param.t};
  char LRL7_types[] = "RLR";
  if (LRL7_param.flag &&
      !SetRSP(LRL7_lengths, LRL7_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRL7_param";
    return false;
  }

  RSPParam LRL8_param;
  LRL(-xb, -yb, phi, &LRL8_param);
  double LRL8_lengths[3] = {-LRL8_param.v, -LRL8_param.u, -LRL8_param.t};
  char LRL8_types[] = "RLR";
  if (LRL8_param.flag &&
      !SetRSP(LRL8_lengths, LRL8_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRL8_param";
    return false;
  }
  return true;
}

bool ReedShepp::CCCC(double x, double y, double phi,
                     std::vector<ReedSheppPath>* all_possible_paths) {
  RSPParam LRLRn1_param;
  LRLRn(x, y, phi, &LRLRn1_param);
  double LRLRn1_lengths[4] = {LRLRn1_param.t, LRLRn1_param.u, -LRLRn1_param.u,
                              LRLRn1_param.v};
  char LRLRn1_types[] = "LRLR";
  if (LRLRn1_param.flag &&
      !SetRSP(LRLRn1_lengths, LRLRn1_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRn_param";
    return false;
  }

  RSPParam LRLRn2_param;
  LRLRn(-x, y, -phi, &LRLRn2_param);
  double LRLRn2_lengths[4] = {-LRLRn2_param.t, -LRLRn2_param.u, LRLRn2_param.u,
                              -LRLRn2_param.v};
  char LRLRn2_types[] = "LRLR";
  if (LRLRn2_param.flag &&
      !SetRSP(LRLRn2_lengths, LRLRn2_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRn2_param";
    return false;
  }

  RSPParam LRLRn3_param;
  LRLRn(x, -y, -phi, &LRLRn3_param);
  double LRLRn3_lengths[4] = {LRLRn3_param.t, LRLRn3_param.u, -LRLRn3_param.u,
                              LRLRn3_param.v};
  char LRLRn3_types[] = "RLRL";
  if (LRLRn3_param.flag &&
      !SetRSP(LRLRn3_lengths, LRLRn3_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRn3_param";
    return false;
  }

  RSPParam LRLRn4_param;
  LRLRn(-x, -y, phi, &LRLRn4_param);
  double LRLRn4_lengths[4] = {-LRLRn4_param.t, -LRLRn4_param.u, LRLRn4_param.u,
                              -LRLRn4_param.v};
  char LRLRn4_types[] = "RLRL";
  if (LRLRn4_param.flag &&
      !SetRSP(LRLRn4_lengths, LRLRn4_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRn4_param";
    return false;
  }

  RSPParam LRLRp1_param;
  LRLRp(x, y, phi, &LRLRp1_param);
  double LRLRp1_lengths[4] = {LRLRp1_param.t, LRLRp1_param.u, LRLRp1_param.u,
                              LRLRp1_param.v};
  char LRLRp1_types[] = "LRLR";
  if (LRLRp1_param.flag &&
      !SetRSP(LRLRp1_lengths, LRLRp1_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRp1_param";
    return false;
  }

  RSPParam LRLRp2_param;
  LRLRp(-x, y, -phi, &LRLRp2_param);
  double LRLRp2_lengths[4] = {-LRLRp2_param.t, -LRLRp2_param.u, -LRLRp2_param.u,
                              -LRLRp2_param.v};
  char LRLRp2_types[] = "LRLR";
  if (LRLRp2_param.flag &&
      !SetRSP(LRLRp2_lengths, LRLRp2_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRp2_param";
    return false;
  }

  RSPParam LRLRp3_param;
  LRLRp(x, -y, -phi, &LRLRp3_param);
  double LRLRp3_lengths[4] = {LRLRp3_param.t, LRLRp3_param.u, LRLRp3_param.u,
                              LRLRp3_param.v};
  char LRLRp3_types[] = "RLRL";
  if (LRLRp3_param.flag &&
      !SetRSP(LRLRp3_lengths, LRLRp3_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRp3_param";
    return false;
  }

  RSPParam LRLRp4_param;
  LRLRp(-x, -y, phi, &LRLRp4_param);
  double LRLRp4_lengths[4] = {-LRLRp4_param.t, -LRLRp4_param.u, -LRLRp4_param.u,
                              -LRLRp4_param.v};
  char LRLRp4_types[] = "RLRL";
  if (LRLRp4_param.flag &&
      !SetRSP(LRLRp4_lengths, LRLRp4_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRp4_param";
    return false;
  }
  return true;
}

bool ReedShepp::CCSC(double x, double y, double phi,
                     std::vector<ReedSheppPath>* all_possible_paths) {
  RSPParam LRSL1_param;
  LRLRn(x, y, phi, &LRSL1_param);
  double LRSL1_lengths[4] = {LRSL1_param.t, -0.5 * M_PI, -LRSL1_param.u,
                             LRSL1_param.v};
  char LRSL1_types[] = "LRSL";
  if (LRSL1_param.flag &&
      !SetRSP(LRSL1_lengths, LRSL1_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSL1_param";
    return false;
  }

  RSPParam LRSL2_param;
  LRLRn(-x, y, -phi, &LRSL2_param);
  double LRSL2_lengths[4] = {-LRSL2_param.t, 0.5 * M_PI, -LRSL2_param.u,
                             -LRSL2_param.v};
  char LRSL2_types[] = "LRSL";
  if (LRSL2_param.flag &&
      !SetRSP(LRSL2_lengths, LRSL2_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSL2_param";
    return false;
  }

  RSPParam LRSL3_param;
  LRLRn(x, -y, -phi, &LRSL3_param);
  double LRSL3_lengths[4] = {LRSL3_param.t, -0.5 * M_PI, LRSL3_param.u,
                             LRSL3_param.v};
  char LRSL3_types[] = "RLSR";
  if (LRSL3_param.flag &&
      !SetRSP(LRSL3_lengths, LRSL3_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSL3_param";
    return false;
  }

  RSPParam LRSL4_param;
  LRLRn(-x, -y, phi, &LRSL4_param);
  double LRSL4_lengths[4] = {-LRSL4_param.t, -0.5 * M_PI, -LRSL4_param.u,
                             -LRSL4_param.v};
  char LRSL4_types[] = "RLSR";
  if (LRSL4_param.flag &&
      !SetRSP(LRSL4_lengths, LRSL4_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSL4_param";
    return false;
  }

  RSPParam LRSR1_param;
  LRLRp(x, y, phi, &LRSR1_param);
  double LRSR1_lengths[4] = {LRSR1_param.t, -0.5 * M_PI, LRSR1_param.u,
                             LRSR1_param.v};
  char LRSR1_types[] = "LRSR";
  if (LRSR1_param.flag &&
      !SetRSP(LRSR1_lengths, LRSR1_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSR1_param";
    return false;
  }

  RSPParam LRSR2_param;
  LRLRp(-x, y, -phi, &LRSR2_param);
  double LRSR2_lengths[4] = {-LRSR2_param.t, 0.5 * M_PI, -LRSR2_param.u,
                             -LRSR2_param.v};
  char LRSR2_types[] = "LRSR";
  if (LRSR2_param.flag &&
      !SetRSP(LRSR2_lengths, LRSR2_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSR2_param";
    return false;
  }

  RSPParam LRSR3_param;
  LRLRp(x, -y, -phi, &LRSR3_param);
  double LRSR3_lengths[4] = {LRSR3_param.t, -0.5 * M_PI, LRSR3_param.u,
                             LRSR3_param.v};
  char LRSR3_types[] = "RLSL";
  if (LRSR3_param.flag &&
      !SetRSP(LRSR3_lengths, LRSR3_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSR3_param";
    return false;
  }

  RSPParam LRSR4_param;
  LRLRp(-x, -y, phi, &LRSR4_param);
  double LRSR4_lengths[4] = {-LRSR4_param.t, 0.5 * M_PI, -LRSR4_param.u,
                             -LRSR4_param.v};
  char LRSR4_types[] = "RLSL";
  if (LRSR4_param.flag &&
      !SetRSP(LRSR4_lengths, LRSR4_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSR4_param";
    return false;
  }

  // backward
  double xb = x * std::cos(phi) + y * std::sin(phi);
  double yb = x * std::sin(phi) - y * std::cos(phi);

  RSPParam LRSL5_param;
  LRLRn(xb, yb, phi, &LRSL5_param);
  double LRSL5_lengths[4] = {LRSL5_param.v, LRSL5_param.u, -0.5 * M_PI,
                             LRSL5_param.t};
  char LRSL5_types[] = "LSRL";
  if (LRSL5_param.flag &&
      !SetRSP(LRSL5_lengths, LRSL5_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRLRn_param";
    return false;
  }

  RSPParam LRSL6_param;
  LRLRn(-xb, yb, -phi, &LRSL6_param);
  double LRSL6_lengths[4] = {-LRSL6_param.v, -LRSL6_param.u, 0.5 * M_PI,
                             -LRSL6_param.t};
  char LRSL6_types[] = "LSRL";
  if (LRSL6_param.flag &&
      !SetRSP(LRSL6_lengths, LRSL6_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSL6_param";
    return false;
  }

  RSPParam LRSL7_param;
  LRLRn(xb, -yb, -phi, &LRSL7_param);
  double LRSL7_lengths[4] = {LRSL7_param.v, LRSL7_param.u, -0.5 * M_PI,
                             LRSL7_param.t};
  char LRSL7_types[] = "RSLR";
  if (LRSL7_param.flag &&
      !SetRSP(LRSL7_lengths, LRSL7_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSL7_param";
    return false;
  }

  RSPParam LRSL8_param;
  LRLRn(-xb, -yb, phi, &LRSL8_param);
  double LRSL8_lengths[4] = {-LRSL8_param.v, -LRSL8_param.u, 0.5 * M_PI,
                             -LRSL8_param.t};
  char LRSL8_types[] = "RSLR";
  if (LRSL8_param.flag &&
      !SetRSP(LRSL8_lengths, LRSL8_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSL8_param";
    return false;
  }

  RSPParam LRSR5_param;
  LRLRp(xb, yb, phi, &LRSR5_param);
  double LRSR5_lengths[4] = {LRSR5_param.v, LRSR5_param.u, -0.5 * M_PI,
                             LRSR5_param.t};
  char LRSR5_types[] = "RSRL";
  if (LRSR5_param.flag &&
      !SetRSP(LRSR5_lengths, LRSR5_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSR5_param";
    return false;
  }

  RSPParam LRSR6_param;
  LRLRp(-xb, yb, -phi, &LRSR6_param);
  double LRSR6_lengths[4] = {-LRSR6_param.v, -LRSR6_param.u, 0.5 * M_PI,
                             -LRSR6_param.t};
  char LRSR6_types[] = "RSRL";
  if (LRSR6_param.flag &&
      !SetRSP(LRSR6_lengths, LRSR6_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSR6_param";
    return false;
  }

  RSPParam LRSR7_param;
  LRLRp(xb, -yb, -phi, &LRSR7_param);
  double LRSR7_lengths[4] = {LRSR7_param.v, LRSR7_param.u, -0.5 * M_PI,
                             LRSR7_param.t};
  char LRSR7_types[] = "LSLR";
  if (LRSR7_param.flag &&
      !SetRSP(LRSR7_lengths, LRSR7_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSR7_param";
    return false;
  }

  RSPParam LRSR8_param;
  LRLRp(-xb, -yb, phi, &LRSR8_param);
  double LRSR8_lengths[4] = {-LRSR8_param.v, -LRSR8_param.u, 0.5 * M_PI,
                             -LRSR8_param.t};
  char LRSR8_types[] = "LSLR";
  if (LRSR8_param.flag &&
      !SetRSP(LRSR8_lengths, LRSR8_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSR8_param";
    return false;
  }
  return true;
}
bool ReedShepp::CCSCC(double x, double y, double phi,
                      std::vector<ReedSheppPath>* all_possible_paths) {
  RSPParam LRSLR1_param;
  LRSLR(x, y, phi, &LRSLR1_param);
  double LRSLR1_lengths[5] = {LRSLR1_param.t, -0.5 * M_PI, LRSLR1_param.u,
                              -0.5 * M_PI, LRSLR1_param.v};
  char LRSLR1_types[] = "LRSLR";
  if (LRSLR1_param.flag &&
      !SetRSP(LRSLR1_lengths, LRSLR1_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSLR1_param";
    return false;
  }

  RSPParam LRSLR2_param;
  LRSLR(-x, y, -phi, &LRSLR2_param);
  double LRSLR2_lengths[5] = {-LRSLR2_param.t, 0.5 * M_PI, -LRSLR2_param.u,
                              0.5 * M_PI, -LRSLR2_param.v};
  char LRSLR2_types[] = "LRSLR";
  if (LRSLR2_param.flag &&
      !SetRSP(LRSLR2_lengths, LRSLR2_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSLR2_param";
    return false;
  }

  RSPParam LRSLR3_param;
  LRSLR(x, -y, -phi, &LRSLR3_param);
  double LRSLR3_lengths[5] = {LRSLR3_param.t, -0.5 * M_PI, LRSLR3_param.u,
                              -0.5 * M_PI, LRSLR3_param.v};
  char LRSLR3_types[] = "RLSRL";
  if (LRSLR3_param.flag &&
      !SetRSP(LRSLR3_lengths, LRSLR3_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSLR3_param";
    return false;
  }

  RSPParam LRSLR4_param;
  LRSLR(-x, -y, phi, &LRSLR4_param);
  double LRSLR4_lengths[5] = {-LRSLR4_param.t, 0.5 * M_PI, -LRSLR4_param.u,
                              0.5 * M_PI, -LRSLR4_param.v};
  char LRSLR4_types[] = "RLSRL";
  if (LRSLR4_param.flag &&
      !SetRSP(LRSLR4_lengths, LRSLR4_types, all_possible_paths)) {
    AINFO << "Fail at SetRSP with LRSLR4_param";
    return false;
  }
  return true;
}

void ReedShepp::LSL(double x, double y, double phi, RSPParam* param) {
  std::pair<double, double> polar =
      common::math::Cartesian2Polar(x - std::sin(phi), y - 1.0 + std::cos(phi));
  double u = polar.first;
  double t = polar.second;
  double v;
  if (t >= 0.0) {
    v = common::math::NormalizeAngle(phi - t);
    if (v >= 0.0) {
      param->flag = true;
      param->u = u;
      param->t = t;
      param->v = v;
    }
  }
}

void ReedShepp::LSR(double x, double y, double phi, RSPParam* param) {
  std::pair<double, double> polar =
      common::math::Cartesian2Polar(x + std::sin(phi), y - 1.0 - std::cos(phi));
  double u1 = polar.first * polar.first;
  double t1 = polar.second;
  double u = 0.0;
  double theta = 0.0;
  double t = 0.0;
  double v = 0.0;
  if (u1 >= 4.0) {
    u = std::sqrt(u1 - 4.0);
    theta = std::atan2(2.0, u);
    t = common::math::NormalizeAngle(t1 + theta);
    v = common::math::NormalizeAngle(t - phi);
    if (t >= 0.0 && v >= 0.0) {
      param->flag = true;
      param->u = u;
      param->t = t;
      param->v = v;
    }
  }
}

void ReedShepp::LRL(double x, double y, double phi, RSPParam* param) {
  std::pair<double, double> polar =
      common::math::Cartesian2Polar(x - std::sin(phi), y - 1.0 + std::cos(phi));
  double u1 = polar.first;
  double t1 = polar.second;
  double u = 0.0;
  double t = 0.0;
  double v = 0.0;
  if (u1 <= 4.0) {
    u = -2.0 * std::asin(0.25 * u1);
    t = common::math::NormalizeAngle(t1 + 0.5 * u + M_PI);
    v = common::math::NormalizeAngle(phi - t + u);
    if (t >= 0.0 && u <= 0.0) {
      param->flag = true;
      param->u = u;
      param->t = t;
      param->v = v;
    }
  }
}

void ReedShepp::SLS(double x, double y, double phi, RSPParam* param) {
  double phi_mod = common::math::NormalizeAngle(phi);
  double xd = 0.0;
  double u = 0.0;
  double t = 0.0;
  double v = 0.0;
  if (y > 0.0 && phi_mod > 0.0 && phi_mod < M_PI * 0.99) {
    xd = -y / std::tan(phi_mod) + x;
    t = xd - std::tan(phi_mod / 2.0);
    u = phi_mod;
    v = std::sqrt((x - xd) * (x - xd) + y * y) - tan(phi_mod / 2.0);
    param->flag = true;
    param->u = u;
    param->t = t;
    param->v = v;
  } else if (y < 0.0 && phi_mod > 0.0 && phi_mod < M_PI * 0.99) {
    xd = -y / std::tan(phi_mod) + x;
    t = xd - std::tan(phi_mod / 2.0);
    u = phi_mod;
    v = -std::sqrt((x - xd) * (x - xd) + y * y) - std::tan(phi_mod / 2.0);
    param->flag = true;
    param->u = u;
    param->t = t;
    param->v = v;
  }
}

void ReedShepp::LRLRn(double x, double y, double phi, RSPParam* param) {
  double xi = x + std::sin(phi);
  double eta = y - 1.0 - std::cos(phi);
  double rho = 0.25 * (2.0 + std::sqrt(xi * xi + eta * eta));
  double u = 0.0;
  double v = 0.0;
  double t = 0.0;
  if (rho <= 1.0) {
    u = std::acos(rho);
    std::pair<double, double> tau_omega = calc_tau_omega(u, v, xi, eta, phi);
    if (tau_omega.first >= 0.0 && tau_omega.second <= 0.0) {
      param->flag = true;
      param->u = u;
      param->t = t;
      param->v = v;
    }
  }
}

void ReedShepp::LRLRp(double x, double y, double phi, RSPParam* param) {
  double xi = x + std::sin(phi);
  double eta = y - 1.0 - std::cos(phi);
  double rho = (20.0 - xi * xi - eta * eta) / 16.0;
  double u = 0.0;
  double v = 0.0;
  double t = 0.0;
  if (rho <= 1.0 && rho >= 0.0) {
    u = -std::acos(rho);
    if (u >= -0.5 * M_PI) {
      std::pair<double, double> tau_omega = calc_tau_omega(u, v, xi, eta, phi);
      if (tau_omega.first >= 0.0 && tau_omega.second >= 0.0) {
        param->flag = true;
        param->u = u;
        param->t = t;
        param->v = v;
      }
    }
  }
}

void ReedShepp::LRSR(double x, double y, double phi, RSPParam* param) {
  double xi = x + std::sin(phi);
  double eta = y - 1.0 - std::cos(phi);
  std::pair<double, double> polar = common::math::Cartesian2Polar(-eta, xi);
  double rho = polar.first;
  double theta = polar.second;
  double t = 0.0;
  double u = 0.0;
  double v = 0.0;
  if (rho >= 2.0) {
    t = theta;
    u = 2.0 - rho;
    v = common::math::NormalizeAngle(t + 0.5 * M_PI - phi);
    if (t >= 0.0 && u <= 0.0 && v <= 0.0) {
      param->flag = true;
      param->u = u;
      param->t = t;
      param->v = v;
    }
  }
}

void ReedShepp::LRSL(double x, double y, double phi, RSPParam* param) {
  double xi = x - std::sin(phi);
  double eta = y - 1.0 + std::cos(phi);
  std::pair<double, double> polar = common::math::Cartesian2Polar(xi, eta);
  double rho = polar.first;
  double theta = polar.second;
  double r = 0.0;
  double t = 0.0;
  double u = 0.0;
  double v = 0.0;

  if (rho >= 2.0) {
    r = std::sqrt(rho * rho - 4.0);
    u = 2.0 - r;
    t = common::math::NormalizeAngle(theta + std::atan2(r, -2.0));
    v = common::math::NormalizeAngle(phi - 0.5 * M_PI - t);
    if (t >= 0.0 && u <= 0.0 && v <= 0.0) {
      param->flag = true;
      param->u = u;
      param->t = t;
      param->v = v;
    }
  }
}

void ReedShepp::LRSLR(double x, double y, double phi, RSPParam* param) {
  double xi = x + std::sin(phi);
  double eta = y - 1.0 - std::cos(phi);
  std::pair<double, double> polar = common::math::Cartesian2Polar(xi, eta);
  double rho = polar.first;
  double t = 0.0;
  double u = 0.0;
  double v = 0.0;
  if (rho >= 2.0) {
    u = 4.0 - std::sqrt(rho * rho - 4.0);
    if (u <= 0.0) {
      t = common::math::NormalizeAngle(
          atan2((4.0 - u) * xi - 2.0 * eta, -2.0 * xi + (u - 4.0) * eta));
      v = common::math::NormalizeAngle(t - phi);

      if (t >= 0.0 && v >= 0.0) {
        param->flag = true;
        param->u = u;
        param->t = t;
        param->v = v;
      }
    }
  }
}

bool ReedShepp::SetRSP(double lengths[], char types[],
                       std::vector<ReedSheppPath>* all_possible_paths) {
  ReedSheppPath path;
  std::size_t lengths_size = sizeof(lengths) / sizeof(lengths[0]);
  std::vector<double> length_vec(lengths, lengths + lengths_size);
  std::vector<char> type_vec(types, types + lengths_size);
  path.segs_lengths = length_vec;
  path.segs_types = type_vec;;
  double sum = 0.0;
  for (std::size_t i = 0; i < lengths_size; i++) {
    sum += std::abs(lengths[i]);
  }
  path.total_length = sum;
  if (path.total_length < 0.0) {
    AINFO << "total length smaller than 0";
    return false;
  }
  all_possible_paths->emplace_back(path);
  return true;
}

bool ReedShepp::GenerateLocalConfigurations(
    const std::shared_ptr<Node3d> start_node,
    const std::shared_ptr<Node3d> end_node,
    std::vector<ReedSheppPath>* all_possible_paths) {
  for (auto path : *all_possible_paths) {
    std::size_t point_num = path.total_length / open_space_conf_.step_size() +
                            path.segs_lengths.size() + 3;
    std::vector<double> px(point_num, 0.0);
    std::vector<double> py(point_num, 0.0);
    std::vector<double> pphi(point_num, 0.0);
    std::vector<bool> pgear(point_num, true);
    double index = 1;
    double d = 0.0;
    double pd = 0.0;
    double ll = 0.0;

    if (path.segs_lengths.at(0) > 0.0) {
      pgear.at(0) = true;
      d = open_space_conf_.step_size();
    } else {
      pgear.at(0) = false;
      d = -open_space_conf_.step_size();
    }
    pd = d;

    for (std::size_t i = 0; i < path.segs_types.size(); i++) {
      char m = path.segs_types.at(i);
      double l = path.segs_lengths.at(i);
      if (l > 0.0) {
        d = open_space_conf_.step_size();
      } else {
        d = -open_space_conf_.step_size();
      }
      double ox = px.at(index);
      double oy = py.at(index);
      double ophi = pphi.at(index);
      index--;
      if (i >= 1 && path.segs_lengths[i - 1] * path.segs_lengths.at(i) > 0) {
        pd = -d - ll;
      } else {
        pd = d - ll;
      }
      while (std::abs(pd) <= std::abs(l)) {
        index++;
        Interpolation(index, pd, m, ox, oy, ophi, px, py, pphi, pgear);
        pd += d;
      }
      ll = l - pd - d;
      index += 1;
      Interpolation(index, l, m, ox, oy, ophi, px, py, pphi, pgear);
    }

    while (px.back() == 0.0) {
      px.pop_back();
      py.pop_back();
      pphi.pop_back();
      pgear.pop_back();
    }

    for (std::size_t i = 0; i < px.size(); i++) {
      path.x.push_back(std::cos(-start_node->GetPhi()) * px.at(i) +
                        std::sin(-start_node->GetPhi()) * py.at(i) +
                        start_node->GetX());
      path.y.push_back(-std::sin(-start_node->GetPhi()) * px.at(i) +
                        std::cos(-start_node->GetPhi()) * py.at(i) +
                        start_node->GetY());
      path.phi.push_back(
          common::math::NormalizeAngle(pphi.at(i) + start_node->GetPhi()));
    }
    path.gear = pgear;
    for (std::size_t i = 0; i < path.segs_lengths.size(); i++) {
      path.segs_lengths.at(i) = path.segs_lengths.at(i) / max_kappa_;
    }
    path.total_length = path.total_length / max_kappa_;
  }
  return true;
}

void ReedShepp::Interpolation(double index, double pd, char m, double ox, double oy,
                   double ophi, std::vector<double>& px,
                   std ::vector<double>& py, std::vector<double>& pphi,
                   std::vector<bool>& pgear) {
  double ldx = 0.0;
  double ldy = 0.0;
  double gdx = 0.0;
  double gdy = 0.0;

  if (m == 'S') {
    px.at(index) = ox + pd / max_kappa_ * std::cos(ophi);
    py.at(index) = oy + pd / max_kappa_ * std::sin(ophi);
    pphi.at(index) = ophi;
  } else {
    ldx = std::sin(pd) / max_kappa_;
    if (m == 'L') {
      ldy = (1.0 - std::cos(pd)) / max_kappa_;
    } else if (m == 'R') {
      ldy = (1.0 - std::cos(pd)) / -max_kappa_;
    }
    gdx = std::cos(-ophi) * ldx + std::sin(-ophi) * ldy;
    gdy = -std::sin(-ophi) * ldx + std::cos(-ophi) * ldy;
    px.at(index) = ox + gdx;
    py.at(index) = oy + gdy;
  }

  if (pd > 0.0) {
    pgear.at(index) = true;
  } else {
    pgear.at(index) = false;
  }

  if (m == 'L') {
    pphi.at(index) = ophi + pd;
  } else if (m == 'R') {
    pphi.at(index) = ophi - pd;
  }
}

}  // namespace planning
}  // namespace apollo