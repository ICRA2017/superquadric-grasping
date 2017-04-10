
/*
 * Copyright (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Giulia Vezzani
 * email:  giulia.vezzani@iit.it
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#include <csignal>
#include <cmath>
#include <limits>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <deque>

#include <IpTNLP.hpp>
#include <IpIpoptApplication.hpp>
#include <IpReturnCodes.hpp>

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/math/Math.h>
#include <yarp/math/SVD.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;

class grasping_NLP : public Ipopt::TNLP
{
    double aux_objvalue;
    Vector obj;
    Vector x_v;
    Matrix bounds;    
    Vector x0;
    deque<Vector> points_on;
    Matrix H_o2w;
    Matrix H_h2w;
    Matrix H_x;
    Vector euler;
    Vector displacement;

public:
    Vector hand;
    Vector object;
    Matrix plane;
    Vector solution;
    Vector robot_pose;
    Matrix bounds_constr;
    string l_o_r;

    /****************************************************************/
    void init(Vector &objectext, Vector &handext, int &n_handpoints, const string &str_hand)
    {
        hand=handext;
        object=objectext;
        yInfo()<<" Object to be grasp: "<<objectext.toString();

        yInfo()<<" Initial hand model: "<<handext.toString();

        H_o2w.resize(4,4);
        H_h2w.resize(4,4);
        H_x.resize(4,4);
        euler.resize(3,0.0);

        euler[0]=object[8];
        euler[1]=object[9];
        euler[2]=object[10];
        H_o2w=euler2dcm(euler);
        euler[0]=object[5];
        euler[1]=object[6];
        euler[2]=object[7];
        H_o2w.setSubcol(euler,0,3);

        euler[0]=hand[8];
        euler[1]=hand[9];
        euler[2]=hand[10];
        H_h2w=euler2dcm(euler);
        euler[0]=hand[5];
        euler[1]=hand[6];
        euler[2]=hand[7];
        H_h2w.setSubcol(euler,0,3);

        yInfo()<<" "<<n_handpoints<<" points on hand:";

        for(int i=0; i<n_handpoints; i++)
        {
            points_on.push_back(computePointsHand(hand,i, n_handpoints, str_hand));
            //cout<<points_on[i].toString()<<endl;
        }

        aux_objvalue=0.0;
    }

    /****************************************************************/
    void checkZbound()
    {
        Vector obj_rot(3,0.0);
        Vector aux(4,0.0);
        Vector aux2(4,0.0);

        aux2[0]=object[0];
        aux2[3]=1;
        aux=H_o2w*aux2;
        obj_rot[0]=norm(aux.subVector(0,2)-object.subVector(5,7));

        aux2[0]=0;
        aux2[1]=object[1];
        aux2[3]=1;
        aux=H_o2w*aux2;
        obj_rot[1]=norm(aux.subVector(0,2)-object.subVector(5,7));

        aux2[1]=0;
        aux2[2]=object[2];
        aux2[3]=1;
        aux=H_o2w*aux2;
        obj_rot[2]=norm(aux.subVector(0,2)-object.subVector(5,7));

        if(hand[1]<obj_rot[2])
            bounds(2,0)=object[7];
        else
        {
            if (findMax(obj_rot)==obj_rot[2])
                bounds(2,0)=object[7]+hand[1]-obj_rot[2];
            else
                bounds(2,0)=object[7]+hand[0]-obj_rot[2];
        }
    }

    /****************************************************************/
    Vector computePointsHand(Vector &hand, int j, int l, const string &str_hand)
    {
        Vector point(3,0.0);
        double theta;

        if (findMax(object.subVector(0,2))> findMax(hand.subVector(0,2)))
            hand[1]=findMax(object.subVector(0,2));

        if (str_hand=="right")
        {
            if (j<l/3)
            {
                theta=j*M_PI/8;
                point[0]=cos(theta)*hand[0];
                point[1]=sin(theta)*hand[1];
                point[2]=0.0;
            }
            if(j>=l/3 && j<l/3*2)
            {
                theta=j*M_PI/16;
                point[0]=cos(theta)*hand[0];
                point[1]=0.0;
                point[2]=sin(theta)*hand[2];
            }
            if(j>=l/3*2 && j<l)
            {
                theta=j*M_PI/16+M_PI;
                point[0]=0.0;
                point[1]=cos(theta)*hand[1];
                point[2]=sin(theta)*hand[2];
            }
        }
        else
        {
            if (j<l/3)
            {
                theta=j*M_PI/8+M_PI;
                point[0]=cos(theta)*hand[0];
                point[1]=sin(theta)*hand[1];
                point[2]=0.0;
            }
            if(j>=l/3 && j<l/3*2)
            {
                theta=j*M_PI/16+M_PI;
                point[0]=cos(theta)*hand[0];
                point[1]=0.0;
                point[2]=sin(theta)*hand[2];
            }
            if(j>=l/3*2 && j<l)
            {
                theta=j*M_PI/16;
                point[0]=0.0;
                point[1]=cos(theta)*hand[1];
                point[2]=sin(theta)*hand[2];
            }

        }
        Vector point_tr(4,0.0);

        euler[0]=hand[8];
        euler[1]=hand[9];
        euler[2]=hand[10];
        H_h2w=euler2dcm(euler);
        euler[0]=hand[5];
        euler[1]=hand[6];
        euler[2]=hand[7];
        H_h2w.setSubcol(euler,0,3);

        Vector point_tmp(4,1.0);
        point_tmp.setSubvector(0,point);
        point_tr=H_h2w*point_tmp;
        point=point_tr.subVector(0,2);

        return point;
    }

    /****************************************************************/
    bool get_nlp_info(Ipopt::Index &n, Ipopt::Index &m,Ipopt::Index &nnz_jac_g,
                      Ipopt::Index &nnz_h_lag, Ipopt::TNLP::IndexStyleEnum &index_style)
    {
        n=6;
        m=6;
        nnz_jac_g=36;
        nnz_h_lag=0;
        index_style=TNLP::C_STYLE;
        x_v.resize(n,0.0);
        bounds.resize(n,2);
        bounds_constr(m,2);

        return true;
    }

    /****************************************************************/
    bool get_bounds_info(Ipopt::Index n, Ipopt::Number *x_l, Ipopt::Number *x_u,
                         Ipopt::Index m, Ipopt::Number *g_l, Ipopt::Number *g_u)
    {
        for (Ipopt::Index i=0; i<n; i++)
        {
           x_l[i]=bounds(i,0);
           x_u[i]=bounds(i,1);
        }

        for (Ipopt::Index i=0; i<m; i++)
        {
           g_l[i]=bounds_constr(i,0);
           g_u[i]=bounds_constr(i,1);
        }

        return true;
    }

    /****************************************************************/
     bool get_starting_point(Ipopt::Index n, bool init_x, Ipopt::Number *x,
                                bool init_z, Ipopt::Number *z_L, Ipopt::Number *z_U,
                                Ipopt::Index m, bool init_lambda, Ipopt::Number *lambda)
     {
         for(Ipopt::Index i=0;i<n;i++)
         {
             x[i]=hand[i+5];
         }

         return true;
     }

     /****************************************************************/
     bool eval_f(Ipopt::Index n, const Ipopt::Number *x, bool new_x,
                    Ipopt::Number &obj_value)
     {
         F(x,points_on,new_x);
         obj_value=aux_objvalue;

         return true;
     }

     /****************************************************************/
     int bigComponent()
     {
         Vector obj_rot(3,0.0);
         Vector aux(4,0.0);
         Vector aux2(4,0.0);

         aux2[0]=object[0];
         aux2[3]=1;
         aux=H_o2w*aux2;
         obj_rot[0]=norm(aux.subVector(0,2)-object.subVector(5,7));

         aux2[0]=0;
         aux2[1]=object[1];
         aux2[3]=1;
         aux=H_o2w*aux2;
         obj_rot[1]=norm(aux.subVector(0,2)-object.subVector(5,7));

         aux2[1]=0;
         aux2[2]=object[2];
         aux2[3]=1;
         aux=H_o2w*aux2;
         obj_rot[2]=norm(aux.subVector(0,2)-object.subVector(5,7));

         int j;
         Vector prod(3,0.0);
         Vector x,y,z;
         x.resize(3,0.0);
         y.resize(3,0.0);
         z.resize(3,0.0);
         x=H_o2w.getCol(0).subVector(0,2);
         y=H_o2w.getCol(1).subVector(0,2);
         z=H_o2w.getCol(2).subVector(0,2);

         aux.resize(3,0.0);
         aux[0]=1;
         prod[0]=abs(x[0]*aux[0]+x[1]*aux[1]+x[2]*aux[2]);
         aux[0]=0; aux[1]=1;
         prod[1]=abs(y[0]*aux[0]+y[1]*aux[1]+y[2]*aux[2]);
         aux[1]=0; aux[2]=1;
         prod[2]=abs(z[0]*aux[0]+z[1]*aux[1]+z[2]*aux[2]);

         for(size_t i=0; i<3; i++)
         {
             if(findMax(prod)==prod[i])
             if (findMax(obj_rot)==obj_rot[i])
                j=i;
         }

         return j;
     }

     /****************************************************************/
     double F(const Ipopt::Number *x, deque<Vector> &points_on, bool new_x)
     {
         double value=0.0;

         for(size_t i=0;i<points_on.size();i++)
             value+= pow( pow(f(object,x,points_on[i]),object[3])-1,2 );

         euler[0]=x[3];
         euler[1]=x[4];
         euler[2]=x[5];
         H_x=euler2dcm(euler);
         euler[0]=x[0];
         euler[1]=x[1];
         euler[2]=x[2];
         H_x.setSubcol(euler,0,3);

         Matrix H(4,4);
         H=H_x*H_h2w;

         Vector xv(3,0.0);
         xv[0]=H(0,3);
         xv[1]=H(1,3);
         xv[2]=H(2,3);
         value*=object[0]*object[1]*object[2]/points_on.size();

         aux_objvalue=value;
     }

     /****************************************************************/
     double f(Vector &obj, const Ipopt::Number *x, Vector &point)
     {
         Matrix H(4,4);

         Vector point_tr(4,0.0);
         Vector point_tmp(4,1.0);
         point_tmp.setSubvector(0,point);

         euler[0]=x[3];
         euler[1]=x[4];
         euler[2]=x[5];
         H_x=euler2dcm(euler);
         euler[0]=x[0];
         euler[1]=x[1];
         euler[2]=x[2];
         H_x.setSubcol(euler,0,3);

         point_tr=H_x*point_tmp;

         double num1=H_o2w(0,0)*point_tr[0]+H_o2w(0,1)*point_tr[1]+H_o2w(0,2)*point_tr[2]-obj[5]*H_o2w(0,0)-obj[6]*H_o2w(0,1)-obj[7]*H_o2w(0,2);
         double num2=H_o2w(1,0)*point_tr[0]+H_o2w(1,1)*point_tr[1]+H_o2w(1,2)*point_tr[2]-obj[5]*H_o2w(1,0)-obj[6]*H_o2w(1,1)-obj[7]*H_o2w(1,2);
         double num3=H_o2w(2,0)*point_tr[0]+H_o2w(2,1)*point_tr[1]+H_o2w(2,2)*point_tr[2]-obj[5]*H_o2w(2,0)-obj[6]*H_o2w(2,1)-obj[7]*H_o2w(2,2);

         double tmp=pow(abs(num1/obj[0]),2.0/obj[4]) + pow(abs(num2/obj[1]),2.0/obj[4]);

         return pow( abs(tmp),obj[4]/obj[3]) + pow( abs(num3/obj[2]),(2.0/obj[3]));
     }

     /****************************************************************/
     double F_v(Vector &x, deque<Vector> &points_on)
     {
         double value=0.0;

         for(size_t i=0;i<points_on.size();i++)
            value+= pow( pow(f_v(object,x,points_on[i]),object[3])-1,2 );

         value*=object[0]*object[1]*object[2]/points_on.size();

         euler[0]=x[3];
         euler[1]=x[4];
         euler[2]=x[5];
         H_x=euler2dcm(euler);
         euler[0]=x[0];
         euler[1]=x[1];
         euler[2]=x[2];
         H_x.setSubcol(euler,0,3);

         Matrix H(4,4);
         H=H_x*H_h2w;

         return value;
     }

      /****************************************************************/
     double f_v(Vector &obj, Vector &x, Vector &point)
     {
         Vector point_tr(4,0.0);
         Vector point_tmp(4,1.0);
         point_tmp.setSubvector(0,point);
         euler[0]=x[3];
         euler[1]=x[4];
         euler[2]=x[5];
         H_x=euler2dcm(euler);
         euler[0]=x[0];
         euler[1]=x[1];
         euler[2]=x[2];
         H_x.setSubcol(euler,0,3);
         Matrix H;
         H.resize(4,4);

         point_tr=H_x*point_tmp;

         double num1=H_o2w(0,0)*point_tr[0]+H_o2w(0,1)*point_tr[1]+H_o2w(0,2)*point_tr[2]-obj[5]*H_o2w(0,0)-obj[6]*H_o2w(0,1)-obj[7]*H_o2w(0,2);
         double num2=H_o2w(1,0)*point_tr[0]+H_o2w(1,1)*point_tr[1]+H_o2w(1,2)*point_tr[2]-obj[5]*H_o2w(1,0)-obj[6]*H_o2w(1,1)-obj[7]*H_o2w(1,2);
         double num3=H_o2w(2,0)*point_tr[0]+H_o2w(2,1)*point_tr[1]+H_o2w(2,2)*point_tr[2]-obj[5]*H_o2w(2,0)-obj[6]*H_o2w(2,1)-obj[7]*H_o2w(2,2);

         double tmp=pow(abs(num1/obj[0]),2.0/obj[4]) + pow(abs(num2/obj[1]),2.0/obj[4]);

         return pow( abs(tmp),obj[4]/obj[3]) + pow( abs(num3/obj[2]),(2.0/obj[3]));
     }

     /********************************************************************/
     double f_v2(Vector &obj, Vector &x, Vector &point_tr)
     {
         double num1=H_o2w(0,0)*point_tr[0]+H_o2w(0,1)*point_tr[1]+H_o2w(0,2)*point_tr[2]-obj[5]*H_o2w(0,0)-obj[6]*H_o2w(0,1)-obj[7]*H_o2w(0,2);
         double num2=H_o2w(1,0)*point_tr[0]+H_o2w(1,1)*point_tr[1]+H_o2w(1,2)*point_tr[2]-obj[5]*H_o2w(1,0)-obj[6]*H_o2w(1,1)-obj[7]*H_o2w(1,2);
         double num3=H_o2w(2,0)*point_tr[0]+H_o2w(2,1)*point_tr[1]+H_o2w(2,2)*point_tr[2]-obj[5]*H_o2w(2,0)-obj[6]*H_o2w(2,1)-obj[7]*H_o2w(2,2);

         double tmp=pow(abs(num1/obj[0]),2.0/obj[4]) + pow(abs(num2/obj[1]),2.0/obj[4]);

         return pow( abs(tmp),obj[4]/obj[3]) + pow( abs(num3/obj[2]),(2.0/obj[3]));
     }

     /****************************************************************/
     bool eval_grad_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                          Ipopt::Number *grad_f)
     {
         Vector x_tmp(6,0.0);
         double grad_p, grad_n;
         double eps=1e-6;

         for(Ipopt::Index i=0;i<n;i++)
            x_tmp[i]=x[i];

         for(Ipopt::Index j=0;j<n;j++)
         {
             x_tmp[j]=x_tmp[j]+eps;

             grad_p=F_v(x_tmp,points_on);

             x_tmp[j]=x_tmp[j]-eps;

             grad_n=F_v(x_tmp,points_on);

             grad_f[j]=(grad_p-grad_n)/eps;
         }

         return true;
     }

     /****************************************************************/
     bool eval_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x,
                 Ipopt::Index m, Ipopt::Number *g)
     {
         euler[0]=x[3];
         euler[1]=x[4];
         euler[2]=x[5];
         H_x=euler2dcm(euler);
         euler[0]=x[0];
         euler[1]=x[1];
         euler[2]=x[2];
         H_x.setSubcol(euler,0,3);


         euler[0]=hand[8];
         euler[1]=hand[9];
         euler[2]=hand[10];
         H_h2w=euler2dcm(euler);
         Matrix H(4,4);
         H=H_x*H_h2w;

         g[0]=H(2,2);
         g[1]=H(0,0);
         g[2]=H(1,2);
         g[4]=H(1,0);

         Vector x_min;
         double minz=10.0;

         for (size_t i=0; i<points_on.size(); i++)
         {
             Vector pnt(4,1.0);
             pnt.setSubvector(0,points_on[i]);
             Vector point=H_x*pnt;

             if (point[2]<minz)
             {
                 minz=point[2];
                 x_min=point;
             }
         }

         g[3]=plane(0,0)*x_min[0]+plane(1,0)*x_min[1]+plane(2,0)*x_min[2]+plane(3,0);

         Vector robotPose(3,0.0);
         Vector x_tmp(6,0.0);
         x_tmp[0]=x[0];
         x_tmp[1]=x[1];
         x_tmp[2]=x[2];
         x_tmp[3]=x[3];
         x_tmp[4]=x[4];
         x_tmp[5]=x[5];

         if (l_o_r=="right")
            robotPose=x_tmp.subVector(0,2)-hand[0]*(H.getCol(2).subVector(0,2));
         else
             robotPose=x_tmp.subVector(0,2)+hand[0]*(H.getCol(2).subVector(0,2));

         g[5]=object[0]*object[1]*object[2]*(pow(f_v2(object,x_tmp, robotPose), object[3]) -1);

         return true;
     }

     /****************************************************************/
     double G_v(Vector &x, int i)
     {
         Vector g(3,0.0);

         Matrix H_x,H;
         H_x.resize(4,4);
         H.resize(4,4);
         euler[0]=x[3];
         euler[1]=x[4];
         euler[2]=x[5];
         H_x=euler2dcm(euler);
         euler[0]=x[0];
         euler[1]=x[1];
         euler[2]=x[2];
         H_x.setSubcol(euler,0,3);

         H=H_x*H_h2w;

         g[0]=H(2,2);
         g[1]=H(0,0);
         g[2]=H(1,2);
         g[4]=H(1,0);

         Vector x_min;
         double minz=10.0;

         for (size_t i1=0; i1<points_on.size(); i1++)
         {
             Vector pnt(4,1.0);
             pnt.setSubvector(0,points_on[i1]);
             Vector point=H_x*pnt;

             if (point[2]<minz)
             {
                 minz=point[2];
                 x_min=point;
             }
         }

         g[3]=plane(0,0)*x_min[0]+plane(1,0)*x_min[1]+plane(2,0)*x_min[2]+plane(3,0);

         Vector robotPose(3,0.0);
         Vector x_tmp(3,0.0);
         x_tmp[0]=x[0];
         x_tmp[1]=x[1];
         x_tmp[2]=x[2];

         if (l_o_r=="right")
            robotPose=x_tmp-hand[0]*(H.getCol(2).subVector(0,2));
         else
             robotPose=x_tmp+hand[0]*(H.getCol(2).subVector(0,2));

         g[5]=object[0]*object[1]*object[2]*(pow(f_v2(object,x_tmp, robotPose), object[3]) -1);

         return g[i];
     }

     /****************************************************************/
     bool eval_jac_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x,
                     Ipopt::Index m, Ipopt::Index nele_jac, Ipopt::Index *iRow,
                     Ipopt::Index *jCol, Ipopt::Number *values)
     {
         Vector x_tmp(6,0.0);
         double grad_p, grad_n;
         double eps=1e-6;

         if(values!=NULL)
         {
             for(Ipopt::Index i=0;i<n;i++)
                x_tmp[i]=x[i];

             int count=0;
             for(Ipopt::Index i=0;i<m; i++)
             {
                 for(Ipopt::Index j=0;j<n;j++)
                 {
                     x_tmp[j]=x_tmp[j]+eps;

                     grad_p=G_v(x_tmp,i);
                     x_tmp[j]=x_tmp[j]-eps;

                     grad_n=G_v(x_tmp,i);

                     values[count]=(grad_p-grad_n)/(eps);
                     count++;
                 }
             }
         }
         else
        {
            jCol[0]=0; jCol[1]=1; jCol[2]=2; jCol[3]=3; jCol[4]=4; jCol[5]=5;
            jCol[6]=0; jCol[7]=1; jCol[8]=2; jCol[9]=3; jCol[10]=4; jCol[11]=5;
            jCol[12]=0; jCol[13]=1; jCol[14]=2;jCol[15]=3; jCol[16]=4; jCol[17]=5;
            jCol[18]=0; jCol[19]=1; jCol[20]=2; jCol[21]=3; jCol[22]=4; jCol[23]=5;
            jCol[24]=0; jCol[25]=1; jCol[26]=2; jCol[27]=3;jCol[28]=4;jCol[29]=5;
            jCol[30]=0; jCol[31]=1; jCol[32]=2; jCol[33]=3;jCol[34]=4;jCol[35]=5;

            iRow[0]=iRow[1]=iRow[2]=iRow[3]=iRow[4]=iRow[5]=0;
            iRow[6]=iRow[7]=iRow[8]=iRow[9]=iRow[10]=iRow[11]=1;
            iRow[12]=iRow[13]=iRow[14]=iRow[15]=iRow[16]=iRow[17]=2;

            iRow[18]=iRow[19]=iRow[20]=iRow[21]=iRow[22]=iRow[23]=3;
            iRow[24]=iRow[25]=iRow[26]=iRow[27]=iRow[28]=iRow[29]=4;
            iRow[30]=iRow[31]=iRow[32]=iRow[33]=iRow[34]=iRow[35]=5;
         }

     return true;

     }

    /****************************************************************/
    void configure(ResourceFinder *rf, const string &left_or_right, const Vector &disp)
    {
        Matrix x0_tmp;
        x0_tmp.resize(6,1);
        x0.resize(6,0.0);
        readMatrix("x0"+left_or_right,x0_tmp,1,rf);
        for(size_t i=0; i< 6; i++)
            x0[i]=x0_tmp(i,0);

        bounds.resize(6,2);
        readMatrix("bounds_"+left_or_right,bounds, 6, rf);
        bounds_constr.resize(6,2);
        readMatrix("bounds_constr_"+left_or_right,bounds_constr,6 , rf);
        plane.resize(4,1);
        readMatrix("plane", plane,4,rf);

        l_o_r=left_or_right;

        if (norm(disp)==0.0 && left_or_right=="right")
        {
            displacement.resize(3,0.0);
            displacement[0]=rf->check("disp_x_right", Value(0.025)).asDouble();
            displacement[1]=rf->check("disp_y_right", Value(0.0)).asDouble();
            displacement[2]=rf->check("disp_z_right", Value(0.0)).asDouble();
        }
        else if (norm(disp)==0.0 && left_or_right=="left")
        {
            displacement.resize(3,0.0);
            displacement[0]=rf->check("disp_x_left", Value(0.025)).asDouble();
            displacement[1]=rf->check("disp_y_left", Value(0.0)).asDouble();
            displacement[2]=rf->check("disp_z_left", Value(0.0)).asDouble();
        }
        else
            displacement=disp;
        yDebug()<<" Displacement "<<displacement.toString();
    }

    /****************************************************************/
   bool readMatrix(const string &tag, Matrix &matrix, const int &dimension, ResourceFinder *rf)
   {
       string tag_x=tag+"_x";
       string tag_y=tag+"_y";
       bool check_x;

       if(tag=="x0" || tag=="plane")
       {
           if (Bottle *b=rf->find(tag.c_str()).asList())
           {
               Vector col;
               if (b->size()>=dimension)
               {
                   for(size_t i=0; i<b->size();i++)
                       col.push_back(b->get(i).asDouble());

                   matrix.setCol(0, col);
               }
               return true;
           }
       }
       else
       {
           if(tag=="bounds_right" || tag=="bounds_constr_right" || tag=="bounds_left" || tag=="bounds_constr_left")
           {
               tag_x=tag+"_l";
               tag_y=tag+"_u";
           }

           if (Bottle *b=rf->find(tag_x.c_str()).asList())
           {
               Vector col;
               if (b->size()>=dimension)
               {
                   for(size_t i=0; i<b->size();i++)
                       col.push_back(b->get(i).asDouble());

                   matrix.setCol(0, col);
               }
               check_x=true;

           }
           if (Bottle *b=rf->find(tag_y.c_str()).asList())
           {
               Vector col;
               if (b->size()>=dimension)
               {
                   for(size_t i=0; i<b->size();i++)
                       col.push_back(b->get(i).asDouble());
                   matrix.setCol(1, col);
               }
               if(check_x==true)
                   return true;
           }
       }
   return false;
   }

   /****************************************************************/
   void finalize_solution(Ipopt::SolverReturn status, Ipopt::Index n,
                          const Ipopt::Number *x, const Ipopt::Number *z_L,
                          const Ipopt::Number *z_U, Ipopt::Index m,
                          const Ipopt::Number *g, const Ipopt::Number *lambda,
                          Ipopt::Number obj_value, const Ipopt::IpoptData *ip_data,
                          Ipopt::IpoptCalculatedQuantities *ip_cq)
   {
       solution.resize(n);

       euler[0]=x[3];
       euler[1]=x[4];
       euler[2]=x[5];
       H_x=euler2dcm(euler);
       euler[0]=x[0];
       euler[1]=x[1];
       euler[2]=x[2];
       H_x.setSubcol(euler,0,3);

       Matrix H;
       H.resize(4,4);
       H=H_x*H_h2w;

       solution.setSubvector(3,dcm2euler(H.transposed()));

       for (Ipopt::Index i=0; i<3; i++)
           solution[i]=H(i,3);

       yInfo()<<" Solution (ellipse pose): "<<solution.toString();

        robot_pose.resize(6,0.0);
        robot_pose.setSubvector(3,dcm2euler(H));
        if (l_o_r=="right")
        {
            robot_pose.setSubvector(0,solution.subVector(0,2)-(hand[0]+displacement[2])*(H.getCol(2).subVector(0,2)));
            robot_pose.setSubvector(0,robot_pose.subVector(0,2)-displacement[0]*(H.getCol(0).subVector(0,2)));
            robot_pose.setSubvector(0,robot_pose.subVector(0,2)-displacement[1]*(H.getCol(1).subVector(0,2)));
        }
        else
        {
            robot_pose.setSubvector(0,solution.subVector(0,2)+(hand[0]+displacement[2])*(H.getCol(2).subVector(0,2)));
            robot_pose.setSubvector(0,robot_pose.subVector(0,2)-displacement[0]*(H.getCol(0).subVector(0,2)));
            robot_pose.setSubvector(0,robot_pose.subVector(0,2)-displacement[1]*(H.getCol(1).subVector(0,2)));
        }


        /*********************/
        /**for(size_t i=0;i<points_on.size();i++)
        {
            Vector point(3,0.0);
            point=points_on[i];

            Vector point_tr(4,0.0);
            Vector point_tmp(4,1.0);
            point_tmp.setSubvector(0,point);

            euler[0]=x[3];
            euler[1]=x[4];
            euler[2]=x[5];
            H_x=euler2dcm(euler);
            euler[0]=x[0];
            euler[1]=x[1];
            euler[2]=x[2];
            H_x.setSubcol(euler,0,3);

            point_tr=H_x*point_tmp;
            cout<<point_tr.subVector(0,2).toString()<<endl;
        }
        /*********************/
   }

   /****************************************************************/
   Vector get_result() const
   {
       return solution;
   }

};



























