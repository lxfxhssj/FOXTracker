#include "KalmanFilter.h"
Pose ExtendKalmanFilter12DOF::on_raw_pose_data(double t, Pose pose) {
    if(!initialized) {
        q = Quaterniond(pose.first);
        T = pose.second;
        w = Eigen::Vector3d::Zero();
        v = Eigen::Vector3d::Zero();
        P = Eigen::Matrix<double, 13, 13>::Identity();

        t0 = t;
        initialized = true;

        std::cout << "Initialized X[" << X.transpose() << "]^T" << std::endl;
        std::cout << "Initialized P" << P.transpose() << std::endl;
        std::cout << "Initialized Q[" << q.coeffs().transpose() << "]T[" << T.transpose() << "]^T" << std::endl;
        return pose;
    }

    Quaterniond zq(pose.first);
    Eigen::Vector3d zT = pose.second;
    Vector7d Z;
    Z.block<4, 1>(0, 0) = zq.coeffs();
    Z.block<3, 1>(4, 0) = zT;

    this->update_cov();
   //First we need to predict to this time
    predict(t);

    auto H = H0mat();
    //std::cout << "h0" << h0().transpose() << std::endl;
    //std::cout << "Z[" << Z.transpose() <<  "]^T" << std::endl;

    //Bug happen on auto on eigen
    Vector7d y = Z - h0();
    //std::cout << "y[" << y.transpose() <<  "]^T\n H\n" <<  H << std::endl;

    Eigen::Matrix<double, 7, 7> S = H*P*H.transpose() + R;
    auto K = P*H.transpose()*S.inverse();

//    std::cout << "W before update" << w << std::endl;
    X = X + K*y;
//    std::cout << "W After" << w << std::endl;

    P = (Eigen::Matrix<double, 13, 13>::Identity() - K * H )*P;

    //std::cout << "Pose Measurement Z[" << Z.transpose();
    //std::cout << "ZT" << Z.block<3, 1>(4, 0).transpose() << std::endl;

    //std::cout << "Residual y[" << y.transpose() << "]^T" << std::endl;
    //    std::cout << "S\n" << S << std::endl;
    //    std::cout << "K\n" << K << std::endl;
    //    std::cout << "P\n" << P << std::endl;
    //std::cout << "X[" << X.transpose() << "]^T" << std::endl;
    //std::cout << "V[" << v.transpose() << "]^T" << std::endl;
    return get_realtime_pose();
}

Pose ExtendKalmanFilter12DOF::predict(double t) {
    Q.setZero();
    double dt = settings->ekf_predict_dt;
    Q.block<4, 4>(0, 0) = Eigen::Matrix4d::Identity() * settings->cov_W*pow(dt, 4)*0.25;//Q for angular velocity
    Q.block<3, 3>(4, 4) = Eigen::Matrix3d::Identity() * settings->cov_V*pow(dt, 4)*0.25;//Q for angular velocity
    R.block<3, 3>(7, 7) = Eigen::Matrix3d::Identity() * settings->cov_W*pow(dt, 2);
    R.block<3, 3>(10, 10) = Eigen::Matrix3d::Identity() * settings->cov_V*pow(dt, 2);

    for (double t1 = this->t_state; t1 < t; t1 += settings->ekf_predict_dt) {
        double dt = settings->ekf_predict_dt;
        if (t - t1 < dt) {
            dt = t - t1;
        }
        this->predict_by_dt(dt);
    }
    this->t_state = t;

    return get_realtime_pose();
}

void ExtendKalmanFilter12DOF::predict_by_dt(double dt){
    auto F = Fmat(dt);
    X = f(dt);
    P = F*P*F.transpose() + Q;
    q.normalize();

//    std::cout << "Predict" << dt <<"s X is [" << X.transpose() << "]^T, P is" << P << std::endl;
}

Eigen::Quaterniond w_dot_q(Eigen::Vector3d omg, Eigen::Quaterniond q) {
    Eigen::Quaterniond w(0, omg.x(), omg.y(), omg.z());
    return w*q;
}


Vector13d ExtendKalmanFilter12DOF::f(double dt) {
    Vector13d _X;
    _X.block<4, 1>(0, 0) = q.coeffs() + 0.5*w_dot_q(w, q).coeffs()*dt;
    _X.block<3, 1>(4, 0) = T + v*dt;
    _X.block<6, 1>(7, 0) = X.block<6, 1>(7, 0);

    return _X;
}

Matrix4d Dwq_by_q(Eigen::Vector3d w, Eigen::Quaterniond q) {
    Matrix4d J;
    J << 0, -w.z(), w.y(), w.x(),
         w.z(), 0, -w.x(), w.y(),
        -w.y(), w.x(), 0, w.z(),
        -w.x(), -w.y(), -w.z(), 0;
    return J;
}

Matrix<double, 4, 3> Dwq_by_w(Eigen::Vector3d omg, Eigen::Quaterniond q) {
    Matrix<double, 4, 3> J;
    J << q.w(), q.z(), -q.y(),
         -q.z(), q.w(), q.x(),
         q.y(), -q.x(), q.w(),
         -q.x(), -q.y(), -q.z();
    return J;
}

Eigen::Matrix<double, 13, 13> ExtendKalmanFilter12DOF::Fmat(double dt) {
    Eigen::Matrix<double, 13, 13> F;
    F.setZero();
    F.block<4, 4>(0, 0) = Matrix4d::Identity() + 0.5*Dwq_by_q(w, q)*dt;
    F.block<4, 3>(7, 0) = 0.5*dt*Dwq_by_w(w, q);
    F.block<3, 3>(4, 4) = Matrix3d::Identity();
    F.block<3, 3>(3, 10) = Matrix3d::Identity() * dt;
    F.block<6, 6>(7, 7) = Matrix<double, 6, 6>::Identity();

    return F;
}



void ExtendKalmanFilter12DOF::update_cov() {
//   R.setOnes();
//   R = 0.001 * R;
   R.setZero();
   R.block<4, 4>(0, 0) = Eigen::Matrix4d::Identity() * settings->cov_Q;
   R.block<3, 3>(4, 4) = Eigen::Matrix3d::Identity() * settings->cov_T;

}
