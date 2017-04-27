/*
 Copyright (c) 2014, Robot Control and Pattern Recognition Group, Warsaw University of Technology
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of the Warsaw University of Technology nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL <COPYright HOLDER> BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef COMMON_CORE_CS_COMPONENTS_COLLISION_DETECTOR_H__
#define COMMON_CORE_CS_COMPONENTS_COLLISION_DETECTOR_H__

#include <sstream>

#include <rtt/Component.hpp>
#include <rtt/Logger.hpp>
#include <rtt/base/PortInterface.hpp>
#include <rtt_rosparam/rosparam.h>

#include "eigen_conversions/eigen_msg.h"

#include <math.h>

#include <collision_convex_model/collision_convex_model.h>
#include <kin_model/kin_model.h>

using namespace RTT;

template<unsigned int N, unsigned int Npairs >
class CollisionDetectorComponent: public RTT::TaskContext {
public:
    explicit CollisionDetectorComponent(const std::string &name);

    bool configureHook();

    bool startHook();

    void stopHook();

    void updateHook();

    std::string getDiag() const;

private:

    typedef Eigen::Matrix<double, N, 1>  Joints;
    typedef boost::array<self_collision::CollisionInfo, Npairs > CollisionList;

    // OROCOS ports
    Joints q_in_;
    RTT::InputPort<Joints > port_q_in_;

    Joints dq_in_;
    RTT::InputPort<Joints > port_dq_in_;

    CollisionList col_out_;
    RTT::OutputPort<CollisionList > port_col_out_;

    // OROCOS properties
    double activation_dist_;
    std::string robot_description_;
    std::string robot_semantic_description_;
    std::vector<std::string > joint_names_;

    std::vector<self_collision::CollisionInfo > col_;
    Eigen::VectorXd q_;

    boost::shared_ptr<self_collision::CollisionModel> col_model_;
    boost::shared_ptr<KinematicModel> kin_model_;
    std::vector<KDL::Frame > links_fk_;
    std::vector<std::string > link_names_vec_;

    int collisions_;

    bool in_collision_;
};

template<unsigned int N, unsigned int Npairs >
CollisionDetectorComponent<N, Npairs >::CollisionDetectorComponent(const std::string &name)
    : TaskContext(name, PreOperational)
    , port_q_in_("q_INPORT")
    , port_dq_in_("dq_INPORT")
    , port_col_out_("col_OUTPORT")
    , activation_dist_(0.0)
    , collisions_(0)
    , in_collision_(false)
{
    this->ports()->addPort(port_q_in_);
    this->ports()->addPort(port_dq_in_);
    this->ports()->addPort(port_col_out_);

    this->addProperty("activation_dist", activation_dist_);
    this->addProperty("robot_description", robot_description_);
    this->addProperty("robot_semantic_description", robot_semantic_description_);
    this->addProperty("joint_names", joint_names_);

    this->addOperation("getDiag", &CollisionDetectorComponent<N, Npairs >::getDiag, this, RTT::ClientThread);
    this->addAttribute("inCollision", in_collision_);

    col_.resize(Npairs);
    q_.resize(N);
}

template<unsigned int N, unsigned int Npairs >
std::string CollisionDetectorComponent<N, Npairs >::getDiag() const {
    static std::vector<self_collision::CollisionInfo > col(Npairs);
    static int l_idx = 0;
    std::ostringstream strs;

    for (int i = 0; i < Npairs; ++i) {
        col[i] = col_out_[i];
    }

    strs << "<cd col_count=\"" << collisions_ << "\">";
    for (int i = 0; i < Npairs; ++i) {
        if (col[i].link1_idx != -1) {
            strs << "<c ";
            strs << "i1=\"" << col[i].link1_idx << "\" ";
            strs << "i2=\"" << col[i].link2_idx << "\" ";
            strs << "p1x=\"" << col[i].p1_B.x() << "\" ";
            strs << "p1y=\"" << col[i].p1_B.y() << "\" ";
            strs << "p1z=\"" << col[i].p1_B.z() << "\" ";
            strs << "p2x=\"" << col[i].p2_B.x() << "\" ";
            strs << "p2y=\"" << col[i].p2_B.y() << "\" ";
            strs << "p2z=\"" << col[i].p2_B.z() << "\" ";
            strs << "d=\"" << col[i].dist << "\" ";
            strs << "n1x=\"" << col[i].n1_B.x() << "\" ";
            strs << "n1y=\"" << col[i].n1_B.y() << "\" ";
            strs << "n1z=\"" << col[i].n1_B.z() << "\" ";
            strs << "n2x=\"" << col[i].n2_B.x() << "\" ";
            strs << "n2y=\"" << col[i].n2_B.y() << "\" ";
            strs << "n2z=\"" << col[i].n2_B.z() << "\" ";
            strs << "/>";
        }
    }

//    for (int i = 0; i < col_model_->getLinksCount(); ++i) {
        const self_collision::Link::VecPtrCollision& vec_col = col_model_->getLinkCollisionArray(l_idx);
        if (vec_col.size() > 0) {
            strs << "<l idx=\"" << l_idx << "\" name=\"" << col_model_->getLinkName(l_idx) << "\">";
            for (int j = 0; j < vec_col.size(); ++j) {
                strs << "<g ";
                strs << "x=\"" << vec_col[j]->origin.p.x() << "\" ";
                strs << "y=\"" << vec_col[j]->origin.p.y() << "\" ";
                strs << "z=\"" << vec_col[j]->origin.p.z() << "\" ";
                strs << "rx=\"" << vec_col[j]->origin.M.GetRot().x() << "\" ";
                strs << "ry=\"" << vec_col[j]->origin.M.GetRot().y() << "\" ";
                strs << "rz=\"" << vec_col[j]->origin.M.GetRot().z() << "\" ";
                
                int type = vec_col[j]->geometry->getType();
                switch (type) {
                case self_collision::Geometry::UNDEFINED:
                    {
                        strs << "type=\"UNDEFINED\" ";
                        break;
                    }
                case self_collision::Geometry::CAPSULE:
                    {
                        strs << "type=\"CAPSULE\" ";
                        boost::shared_ptr<self_collision::Capsule > ob = boost::dynamic_pointer_cast<self_collision::Capsule >(vec_col[j]->geometry);
                        strs << "r=\"" << ob->getRadius() << "\" ";
                        strs << "l=\"" << ob->getLength() << "\" ";
                        break;
                    }
                case self_collision::Geometry::CONVEX:
                    strs << "type=\"CONVEX\" ";
                    break;
                case self_collision::Geometry::SPHERE:
                    {
                        strs << "type=\"SPHERE\" ";
                        boost::shared_ptr<self_collision::Sphere > ob = boost::dynamic_pointer_cast<self_collision::Sphere >(vec_col[j]->geometry);
                        strs << "r=\"" << ob->getRadius() << "\" ";
                        break;
                    }
                case self_collision::Geometry::TRIANGLE:
                    strs << "type=\"TRIANGLE\" ";
                    break;
                case self_collision::Geometry::OCTOMAP:
                    strs << "type=\"OCTOMAP\" ";
                    break;
                default:
                    strs << "type=\"ERROR\" ";
                }
                strs << "/>";
            }
            strs << "</l>";
        }
//    }
    l_idx = (l_idx + 1) % col_model_->getLinksCount();

    strs << "</cd>";

    return strs.str();
}

template<unsigned int N, unsigned int Npairs >
bool CollisionDetectorComponent<N, Npairs >::configureHook() {
    Logger::In in("CollisionDetectorComponent::configureHook");

    // Get the rosparam service requester
    boost::shared_ptr<rtt_rosparam::ROSParam> rosparam = this->getProvider<rtt_rosparam::ROSParam>("rosparam");

      // Get the parameters
    if(!rosparam) {
        Logger::log() << Logger::Error << "Could not get ROS parameters from rtt_rosparam" << Logger::endl;
        return false;
    }

    // Get the ROS parameter "/robot_description"
    if (!rosparam->getAbsolute("robot_description")) {
        Logger::log() << Logger::Error << "could not read ROS parameter \'robot_description\'" << Logger::endl;
        return false;
    }

    if (!rosparam->getAbsolute("robot_semantic_description")) {
        Logger::log() << Logger::Error << "could not read ROS parameter \'robot_semantic_description\'" << Logger::endl;
        return false;
    }

/*
    RTT::OperationCaller<bool()> rosparam_getAbsolute = rosparam->getOperation("getAll");
    if (!tc_rosparam_getAll.ready()) {
        Logger::log() << Logger::Error << "could not get ROS parameter getAll operation for " << tc->getName() << Logger::endl;
        return false;
    }
    if (!tc_rosparam_getAll()) {
        Logger::log() << Logger::Warning << "could not read ROS parameters for " << tc->getName() << Logger::endl;
//        return false;     // TODO: this IS an error
    }
*/

    if (joint_names_.size() != N) {
        Logger::log() << Logger::Error << "ROS parameter \'joint_names\' has wrong size: " << joint_names_.size()
            << ", should be: " << N << Logger::endl;
        return false;
    }

    col_model_ = self_collision::CollisionModel::parseURDF(robot_description_);
    col_model_->parseSRDF(robot_semantic_description_);
    col_model_->generateCollisionPairs();

    links_fk_.resize(col_model_->getLinksCount());

    kin_model_.reset( new KinematicModel(robot_description_, joint_names_) );

    if (activation_dist_ == 0.0) {
        log(RTT::Error) << "Property \'activation_dist\' is not set" << Logger::endl;
        return false;
    }

    link_names_vec_.clear();
    for (int l_idx = 0; l_idx < col_model_->getLinksCount(); l_idx++) {
        link_names_vec_.push_back(col_model_->getLinkName(l_idx));
    }

    return true;
}

template<unsigned int N, unsigned int Npairs >
bool CollisionDetectorComponent<N, Npairs >::startHook() {
    collisions_ = 0;
    in_collision_ = false;
    return true;
}

template<unsigned int N, unsigned int Npairs >
void CollisionDetectorComponent<N, Npairs >::stopHook() {
    collisions_ = 0;
    in_collision_ = false;
}

template<unsigned int N, unsigned int Npairs >
void CollisionDetectorComponent<N, Npairs >::updateHook() {
    //
    // read HW status
    //
    if (port_q_in_.read(q_in_) != RTT::NewData) {
        Logger::In in("CollisionDetectorComponent::updateHook");
        log(RTT::Error) << "no data on port " << port_q_in_.getName() << Logger::endl;
        error();
        return;
    }

    if (port_dq_in_.read(dq_in_) != RTT::NewData) {
        Logger::In in("CollisionDetectorComponent::updateHook");
        log(RTT::Error) << "no data on port " << port_dq_in_.getName() << Logger::endl;
        error();
        return;
    }

    for (int i = 0; i < N; ++i) {
        q_(i) = q_in_(i);
    }

    // calculate forward kinematics for all links
    kin_model_->calculateFkAll(q_);
    for (int l_idx = 0; l_idx < col_model_->getLinksCount(); l_idx++) {
        links_fk_[l_idx] = kin_model_->getFrame(col_model_->getLinkName(l_idx));
    }

    getCollisionPairsNoAlloc(col_model_, links_fk_, activation_dist_, col_);

    int collisions = 0;
    int col_out_idx = 0;

    for (int i = 0; i < Npairs; ++i) {
        if (col_[i].link1_idx == -1) {
            break;
        }

        const KDL::Frame &T_B_L1 = links_fk_[col_[i].link1_idx];
        const std::string &link1_name = link_names_vec_[col_[i].link1_idx];
        const std::string &link2_name = link_names_vec_[col_[i].link2_idx];

        KDL::Frame T_L1_B = T_B_L1.Inverse();
        const KDL::Frame &T_B_L2 = links_fk_[col_[i].link2_idx];
        KDL::Frame T_L2_B = T_B_L2.Inverse();
        KDL::Vector p1_L1 = T_L1_B * col_[i].p1_B;
        KDL::Vector p2_L2 = T_L2_B * col_[i].p2_B;
        KDL::Vector n1_L1 = KDL::Frame(T_L1_B.M) * col_[i].n1_B;
        KDL::Vector n2_L2 = KDL::Frame(T_L2_B.M) * col_[i].n2_B;

        KinematicModel::Jacobian jac1(6, N), jac2(6, N);
        kin_model_->getJacobiansForPairX(jac1, jac2, link1_name, p1_L1, link2_name, p2_L2, q_);
/*
// the commented code may be used in the future (for repulsive forces)
        double depth = (activation_dist_ - col_[i].dist);

        // repulsive force
        double f = 0.0;
        if (col_[i].dist <= activation_dist_) {
            f = depth / activation_dist_;
        }
        else {
            f = 0.0;
        }

        if (f > 1.0) {
            f = 1.0;
        }
        double Frep = Fmax_ * f * f;

        double K = 2.0 * Fmax_ / (activation_dist_ * activation_dist_);
*/
        // the mapping between motions along contact normal and the Cartesian coordinates
        KDL::Vector e1 = KDL::Frame(T_B_L1.M) * n1_L1;
        KDL::Vector e2 = KDL::Frame(T_B_L2.M) * n2_L2;
        Eigen::VectorXd Jd1(3), Jd2(3);
        for (int i = 0; i < 3; i++) {
            Jd1[i] = e1[i];
            Jd2[i] = e2[i];
        }

        KinematicModel::Jacobian jac1_lin(3, N), jac2_lin(3, N);
        for (int q_idx = 0; q_idx < N; q_idx++) {
            for (int row_idx = 0; row_idx < 3; row_idx++) {
                jac1_lin(row_idx, q_idx) = jac1(row_idx, q_idx);
                jac2_lin(row_idx, q_idx) = jac2(row_idx, q_idx);
            }
        }

        KinematicModel::Jacobian Jcol1 = Jd1.transpose() * jac1_lin;
        KinematicModel::Jacobian Jcol2 = Jd2.transpose() * jac2_lin;

        KinematicModel::Jacobian Jcol(1, N);
        for (int q_idx = 0; q_idx < N; q_idx++) {
            Jcol(0, q_idx) = Jcol1(0, q_idx) + Jcol2(0, q_idx);
        }

        // calculate relative velocity between points (1 dof)
        double ddij = (Jcol * dq_in_)(0,0);

        if (ddij > 0.01) {
            col_out_[col_out_idx] = col_[i];
            ++col_out_idx;
            ++collisions;
        }
    }

    for (int i = col_out_idx; i < Npairs; ++i) {
        col_out_[i].link1_idx = -1;
        col_out_[i].link2_idx = -1;
    }

    collisions_ = collisions;

    if (collisions_ > 0) {
        in_collision_ = true;
    }
    else {
        in_collision_ = false;
    }
}

#endif  // COMMON_CORE_CS_COMPONENTS_COLLISION_DETECTOR_H__

