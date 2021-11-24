
#include <fstream>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <vector> 
//#include <random>

#include <bits/stdc++.h>
#include <Eigen/Dense>
#include <Eigen/Core>
#include <Eigen/LU>
#include <Eigen/SVD>
#include <Eigen/QR>
#include "time.h"

#include "genotype.h"
#include "mailman.h"
#include "arguments.h"
//#include "helper.h"
#include "storage.h"

#include "/usr/include/boost/random.hpp"

#if SSE_SUPPORT==1
	#define fastmultiply fastmultiply_sse
	#define fastmultiply_pre fastmultiply_pre_sse
#else
	#define fastmultiply fastmultiply_normal
	#define fastmultiply_pre fastmultiply_pre_normal
#endif

using namespace Eigen;
using namespace std;

// Storing in RowMajor Form
typedef Matrix<double, Dynamic, Dynamic, RowMajor> MatrixXdr;
//typedef Matrix<int, Dynamic, Dynamic, RowMajor> MatrixXdrInt;

class data {

 public:
     MatrixXdr gen;
     int index;

};


//Intermediate Variables
int blocksize;
int hsegsize;
double *partialsums;
double *sum_op;		
double *yint_e;
double *yint_m;
double **y_e;
double **y_m;


struct timespec t0;

clock_t total_begin = clock();
MatrixXdr pheno;
MatrixXdr mask;
MatrixXdr covariate;  
MatrixXdr Q;
MatrixXdr v1; //W^ty
MatrixXdr v2;            //QW^ty
MatrixXdr v3;    //WQW^ty
MatrixXdr new_pheno;



genotype g;
genotype g1;
genotype g2;
MatrixXdr geno_matrix; //(p,n)
genotype* Geno;
int MAX_ITER;
int k,p,n;
int k_orig;

double score;

MatrixXdr c; //(p,k)
MatrixXdr x; //(k,n)
MatrixXdr v; //(p,k)
MatrixXdr means; //(p,1)
MatrixXdr stds; //(p,1)
MatrixXdr sum2;
MatrixXdr sum;  

Eigen::RowVectorXd means_na;
Eigen::RowVectorXd stds_na;
////////
//related to phenotype	
double y_sum; 
double y_mean;

options command_line_opts;

bool debug = false;
bool check_accuracy = false;
bool var_normalize=false;
int accelerated_em=0;
double convergence_limit;
bool memory_efficient = false;
bool missing=false;
bool fast_mode = true;
bool text_version = false;
bool use_cov=false; 


//// jackknife index wich are computed based on annotation file
MatrixXdr dic_index;
MatrixXdr jack_bin_size;
vector<int> len;
vector<int> Annot;
int Njack=100;
int Nbin=160;
int Nz=10;
int cov_num;
int Nindv_mask;
int NC;
///////
MatrixXdr jack;
MatrixXdr point_est;
MatrixXdr enrich_jack;
MatrixXdr enrich_point_est;




//define random vector z's
MatrixXdr  all_zb;
MatrixXdr  all_Uzb;
MatrixXdr res;
MatrixXdr XXz;
MatrixXdr Xy;
MatrixXdr yXXy;


///
//Matrix<int, Dynamic, Dynamic, RowMajor> gen;
MatrixXdr gen;
bool read_header;
//read variables
unsigned char mask2;
int wordsize;
unsigned int unitsperword;
int unitsize;
int nrow, ncol;
unsigned char *gtype;
int Nsnp;
int Nindv;
bool **bin_annot;
int step_size;
int step_size_rem;
std::vector<std::vector<bool> > annot_bool;
std::vector<std::vector<int> > jack_bin;
vector <data> allgen;
vector <genotype> allgen_mail;
int global_snp_index;
bool use_mailman=true;

///reading single col annot
vector <int> SNP_annot;
bool use_1col_annot=false;


///Variables for reg out cov on both side of LM
bool both_side_cov=true;
MatrixXdr UXXz;
MatrixXdr XXUz;
MatrixXdr Xz;
MatrixXdr trVK;

std::istream& newline(std::istream& in)
{
    if ((in >> std::ws).peek() != std::char_traits<char>::to_int_type('\n')) {
        in.setstate(std::ios_base::failbit);
    }
    return in.ignore();
}


int read_cov(bool std,int Nind, std::string filename, std::string covname){
	ifstream ifs(filename.c_str(), ios::in); 
	std::string line; 
	std::istringstream in; 
	int covIndex = 0; 
	std::getline(ifs,line); 
	in.str(line); 
	string b;
	vector<vector<int> > missing; 
	int covNum=0;  
	while(in>>b)
	{
		if(b!="FID" && b !="IID"){
		missing.push_back(vector<int>()); //push an empty row  
		if(b==covname && covname!="")
			covIndex=covNum; 
		covNum++; 
		}
	}
	vector<double> cov_sum(covNum, 0); 
	if(covname=="")
	{
		covariate.resize(Nind, covNum); 
		cout<< "Read in "<<covNum << " Covariates.. "<<endl;
	}
	else 
	{
		covariate.resize(Nind, 1); 
		cout<< "Read in covariate "<<covname<<endl;  
	}

	
	int j=0; 
	while(std::getline(ifs, line)){
		in.clear(); 
		in.str(line);
		string temp;
		in>>temp; in>>temp; //FID IID 
		for(int k=0; k<covNum; k++){
			
			in>>temp;
			if(temp=="NA")
			{
				missing[k].push_back(j);
				continue; 
			} 
			double cur = atof(temp.c_str()); 
			if(cur==-9)
			{
				missing[k].push_back(j); 
				continue; 
			}
			if(covname=="")
			{
				cov_sum[k]= cov_sum[k]+ cur; 
				covariate(j,k) = cur; 
			}
			else
				if(k==covIndex)
				{
					covariate(j, 0) = cur;
					cov_sum[k] = cov_sum[k]+cur; 
				}
		}
		//if(j<10) 
		//	cout<<covariate.block(j,0,1, covNum)<<endl; 
		j++;
	}
	//compute cov mean and impute 
	for (int a=0; a<covNum ; a++)
	{
		int missing_num = missing[a].size(); 
		cov_sum[a] = cov_sum[a] / (Nind - missing_num);

		for(int b=0; b<missing_num; b++)
		{
                        int index = missing[a][b];
                        if(covname=="")
                                covariate(index, a) = cov_sum[a];
                        else if (a==covIndex)
                                covariate(index, 0) = cov_sum[a];
                } 
	}
	if(std)
	{
		MatrixXdr cov_std;
		cov_std.resize(1,covNum);  
		MatrixXdr sum = covariate.colwise().sum();
		MatrixXdr sum2 = (covariate.cwiseProduct(covariate)).colwise().sum();
		MatrixXdr temp;
//		temp.resize(Nind, 1); 
//		for(int i=0; i<Nind; i++)
//			temp(i,0)=1;  
		for(int b=0; b<covNum; b++)
		{
			cov_std(0,b) = sum2(0,b) + Nind*cov_sum[b]*cov_sum[b]- 2*cov_sum[b]*sum(0,b);
			cov_std(0,b) =sqrt((Nind- 1)/cov_std(0,b)) ;
			double scalar=cov_std(0,b); 
			for(int j=0; j<Nind; j++)
			{
				covariate(j,b) = covariate(j,b)-cov_sum[b];  
				covariate(j,b) =covariate(j,b)*scalar;
			} 
			//covariate.col(b) = covariate.col(b) -temp*cov_sum[b];
			
		}
	}	
	return covNum; 
}







void multiply_y_pre_fast(MatrixXdr &op, int Ncol_op ,MatrixXdr &res,bool subtract_means){

        for(int k_iter=0;k_iter<Ncol_op;k_iter++){
                sum_op[k_iter]=op.col(k_iter).sum();
        }

                        //cout << "Nops = " << Ncol_op << "\t" <<g.Nsegments_hori << endl;
        #if DEBUG==1
                if(debug){
                        print_time ();
                        cout <<"Starting mailman on premultiply"<<endl;
                        cout << "Nops = " << Ncol_op << "\t" <<g.Nsegments_hori << endl;
                        cout << "Segment size = " << g.segment_size_hori << endl;
                        cout << "Matrix size = " <<g.segment_size_hori<<"\t" <<g.Nindv << endl;
                        cout << "op = " <<  op.rows () << "\t" << op.cols () << endl;
                }
        #endif


        //TODO: Memory Effecient SSE FastMultipy

        for(int seg_iter=0;seg_iter<g.Nsegments_hori-1;seg_iter++){
                mailman::fastmultiply(g.segment_size_hori,g.Nindv,Ncol_op,g.p[seg_iter],op,yint_m,partialsums,y_m);
                int p_base = seg_iter*g.segment_size_hori;
                for(int p_iter=p_base; (p_iter<p_base+g.segment_size_hori) && (p_iter<g.Nsnp) ; p_iter++ ){
                        for(int k_iter=0;k_iter<Ncol_op;k_iter++)
                                res(p_iter,k_iter) = y_m[p_iter-p_base][k_iter];
                }
        }

        int last_seg_size = (g.Nsnp%g.segment_size_hori !=0 ) ? g.Nsnp%g.segment_size_hori : g.segment_size_hori;
        mailman::fastmultiply(last_seg_size,g.Nindv,Ncol_op,g.p[g.Nsegments_hori-1],op,yint_m,partialsums,y_m);
        int p_base = (g.Nsegments_hori-1)*g.segment_size_hori;
        for(int p_iter=p_base; (p_iter<p_base+g.segment_size_hori) && (p_iter<g.Nsnp) ; p_iter++){
                for(int k_iter=0;k_iter<Ncol_op;k_iter++)
                        res(p_iter,k_iter) = y_m[p_iter-p_base][k_iter];
        }

        #if DEBUG==1
                if(debug){
                        print_time ();
                        cout <<"Ending mailman on premultiply"<<endl;
                }
        #endif


        if(!subtract_means)
                return;

        for(int p_iter=0;p_iter<p;p_iter++){
                for(int k_iter=0;k_iter<Ncol_op;k_iter++){
                        res(p_iter,k_iter) = res(p_iter,k_iter) - (g.get_col_mean(p_iter)*sum_op[k_iter]);
                        if(var_normalize)
                                res(p_iter,k_iter) = res(p_iter,k_iter)/(g.get_col_std(p_iter));
                }
        }

}



void multiply_y_post_fast(MatrixXdr &op_orig, int Nrows_op, MatrixXdr &res,bool subtract_means){

        MatrixXdr op;
        op = op_orig.transpose();

        if(var_normalize && subtract_means){
                for(int p_iter=0;p_iter<p;p_iter++){
                        for(int k_iter=0;k_iter<Nrows_op;k_iter++)
                                op(p_iter,k_iter) = op(p_iter,k_iter) / (g.get_col_std(p_iter));
                }
        }

        #if DEBUG==1
                if(debug){
                        print_time ();
                        cout <<"Starting mailman on postmultiply"<<endl;
                }
        #endif

        int Ncol_op = Nrows_op;

        //cout << "ncol_op = " << Ncol_op << endl;

        int seg_iter;
        for(seg_iter=0;seg_iter<g.Nsegments_hori-1;seg_iter++){
mailman::fastmultiply_pre(g.segment_size_hori,g.Nindv,Ncol_op, seg_iter * g.segment_size_hori, g.p[seg_iter],op,yint_e,partialsums,y_e);
        }
        int last_seg_size = (g.Nsnp%g.segment_size_hori !=0 ) ? g.Nsnp%g.segment_size_hori : g.segment_size_hori;
        mailman::fastmultiply_pre(last_seg_size,g.Nindv,Ncol_op, seg_iter * g.segment_size_hori, g.p[seg_iter],op,yint_e,partialsums,y_e);

        for(int n_iter=0; n_iter<n; n_iter++)  {
                for(int k_iter=0;k_iter<Ncol_op;k_iter++) {
                        res(k_iter,n_iter) = y_e[n_iter][k_iter];
                        y_e[n_iter][k_iter] = 0;
                }
        }

        #if DEBUG==1
                if(debug){
                        print_time ();
                        cout <<"Ending mailman on postmultiply"<<endl;
                }
        #endif


        if(!subtract_means)
                return;

        double *sums_elements = new double[Ncol_op];
        memset (sums_elements, 0, Nrows_op * sizeof(int));

        for(int k_iter=0;k_iter<Ncol_op;k_iter++){
                double sum_to_calc=0.0;
                for(int p_iter=0;p_iter<p;p_iter++)
                        sum_to_calc += g.get_col_mean(p_iter)*op(p_iter,k_iter);
                sums_elements[k_iter] = sum_to_calc;
        }
        for(int k_iter=0;k_iter<Ncol_op;k_iter++){
                for(int n_iter=0;n_iter<n;n_iter++)
                        res(k_iter,n_iter) = res(k_iter,n_iter) - sums_elements[k_iter];
        }


}


void initial_var()
{
    /*if(key==1)
        g=g1;
    if(key==2)
        g=g2;*/
   // g=Geno[key];
        

	p = g.Nsnp;
        n = g.Nindv;


        c.resize(p,k);
        x.resize(k,n);
        v.resize(p,k);
        //means.resize(p,1);
        //stds.resize(p,1);
        sum2.resize(p,1);
        sum.resize(p,1);


        if(!fast_mode && !memory_efficient){
                geno_matrix.resize(p,n);
                g.generate_eigen_geno(geno_matrix,var_normalize);
        }

        //TODO: Initialization of c with gaussian distribution
        c = MatrixXdr::Random(p,k);


        // Initial intermediate data structures
        blocksize = k;
         hsegsize = g.segment_size_hori;        // = log_3(n)
        int hsize = pow(3,hsegsize);
        int vsegsize = g.segment_size_ver;              // = log_3(p)
        int vsize = pow(3,vsegsize);

        partialsums = new double [blocksize];
        sum_op = new double[blocksize];
        yint_e = new double [hsize*blocksize];
        yint_m = new double [hsize*blocksize];
        memset (yint_m, 0, hsize*blocksize * sizeof(double));
        memset (yint_e, 0, hsize*blocksize * sizeof(double));

        y_e  = new double*[g.Nindv];
        for (int i = 0 ; i < g.Nindv ; i++) {
                y_e[i] = new double[blocksize];
                memset (y_e[i], 0, blocksize * sizeof(double));
        }

        y_m = new double*[hsegsize];
        for (int i = 0 ; i < hsegsize ; i++)
                y_m[i] = new double[blocksize];
      /*  for(int i=0;i<p;i++){
                means(i,0) = g.get_col_mean(i);
                stds(i,0) =1/g.get_col_std(i);
                //sum2(i,0) =g.get_col_sum2(i); 
                sum(i,0)= g.get_col_sum(i);
        }

*/


}
 



void read_pheno2(int Nind, std::string filename){
//	pheno.resize(Nind,1); 
	ifstream ifs(filename.c_str(), ios::in); 
	
	std::string line;
	std::istringstream in;  
	int phenocount=0; 
//read header
	std::getline(ifs,line); 
	in.str(line); 
	string b; 
	while(in>>b)
	{
		if(b!="FID" && b !="IID")
			phenocount++; 
	}
	pheno.resize(Nind, phenocount);
	mask.resize(Nind, phenocount);
	int i=0;  
	while(std::getline(ifs, line)){
		in.clear(); 
		in.str(line); 
		string temp;
		//fid,iid
		//todo: fid iid mapping; 
		//todo: handle missing phenotype
		in>>temp; in>>temp; 
		for(int j=0; j<phenocount;j++) {
			in>>temp;
			double cur = atof(temp.c_str());
			if(temp=="NA" || cur==-9){
			pheno(i,j)=0;
			mask(i,j)=0;
			}
			else{
			pheno(i,j)=atof(temp.c_str());
			mask(i,j)=1;

			}

    
		}
		i++;
	}
	//cout<<pheno; 
}

double compute_yXXy(int num_snp){


        MatrixXdr res(num_snp, 1);

	
	if(use_mailman==true)
		multiply_y_pre_fast(pheno,1,res,false);
	else
		 res=gen*pheno;
	

	res = res.cwiseProduct(stds);
        MatrixXdr resid(num_snp, 1);
        resid = means.cwiseProduct(stds);
        resid = resid *y_sum;
        MatrixXdr Xy(num_snp,1);
        Xy = res-resid;
    
        double yXXy = (Xy.array()* Xy.array()).sum();
        
        return yXXy;

}

double compute_yVXXVy(int num_snp){
        MatrixXdr new_pheno_sum = new_pheno.colwise().sum();
        MatrixXdr res(num_snp, 1);
        
	



        if(use_mailman==true)
                multiply_y_pre_fast(new_pheno,1,res,false);
        else
                 res=gen*new_pheno;



        res = res.cwiseProduct(stds);
        MatrixXdr resid(num_snp, 1);
        resid = means.cwiseProduct(stds);
        resid = resid *new_pheno_sum;
        MatrixXdr Xy(num_snp,1);
        Xy = res-resid;
        double ytVXXVy = (Xy.array()* Xy.array()).sum();
        return ytVXXVy;

}





	
MatrixXdr  compute_XXz (int num_snp){
	//mask
	/*for (int i=0;i<Nz;i++)
	   for(int j=0;j<Nindv;j++)
		 all_zb(j,i)=all_zb(j,i)*mask(j,0);
*/
         res.resize(num_snp, Nz);

        
	if(use_mailman==true)
		multiply_y_pre_fast(all_zb,Nz,res, false);
	else	
        	res=gen*all_zb;
   

        MatrixXdr zb_sum = all_zb.colwise().sum();
        

	for(int j=0; j<num_snp; j++)
            for(int k=0; k<Nz;k++)
                res(j,k) = res(j,k)*stds(j,0);
            
        MatrixXdr resid(num_snp, Nz);
        MatrixXdr inter = means.cwiseProduct(stds);
        resid = inter * zb_sum;
        MatrixXdr inter_zb = res - resid;
       

	for(int k=0; k<Nz; k++)
            for(int j=0; j<num_snp;j++)
                inter_zb(j,k) =inter_zb(j,k) *stds(j,0);
       MatrixXdr new_zb = inter_zb.transpose();
       MatrixXdr new_res(Nz, Nindv);
       
	
	 if(use_mailman==true)
	    multiply_y_post_fast(new_zb, Nz, new_res, false);
	 else
	    new_res=new_zb*gen;	

       MatrixXdr new_resid(Nz, num_snp);
       MatrixXdr zb_scale_sum = new_zb * means;
       new_resid = zb_scale_sum * MatrixXdr::Constant(1,Nindv, 1);


                      /// new zb 
       MatrixXdr temp=new_res - new_resid;

	for (int i=0;i<Nz;i++)
           for(int j=0;j<Nindv;j++)
                 temp(i,j)=temp(i,j)*mask(j,0);


	return temp.transpose();
       

}






MatrixXdr  compute_XXUz (int num_snp){
        //mask
/*        for (int i=0;i<Nz;i++)
           for(int j=0;j<Nindv;j++)
                 all_Uzb(j,i)=all_Uzb(j,i)*mask(j,0);
*/
         res.resize(num_snp, Nz);


        if(use_mailman==true)
                multiply_y_pre_fast(all_Uzb,Nz,res, false);
        else
                res=gen*all_Uzb;
  

        MatrixXdr zb_sum = all_Uzb.colwise().sum();


        for(int j=0; j<num_snp; j++)
            for(int k=0; k<Nz;k++)
                res(j,k) = res(j,k)*stds(j,0);

        MatrixXdr resid(num_snp, Nz);
        MatrixXdr inter = means.cwiseProduct(stds);
        resid = inter * zb_sum;
        MatrixXdr inter_zb = res - resid;


        for(int k=0; k<Nz; k++)
            for(int j=0; j<num_snp;j++)
                inter_zb(j,k) =inter_zb(j,k) *stds(j,0);
       MatrixXdr new_zb = inter_zb.transpose();
       MatrixXdr new_res(Nz, Nindv);


         if(use_mailman==true)
            multiply_y_post_fast(new_zb, Nz, new_res, false);
         else
            new_res=new_zb*gen;

       MatrixXdr new_resid(Nz, num_snp);
       MatrixXdr zb_scale_sum = new_zb * means;
       new_resid = zb_scale_sum * MatrixXdr::Constant(1,Nindv, 1);


                      /// new zb 
       MatrixXdr temp=new_res - new_resid;

        for (int i=0;i<Nz;i++)
           for(int j=0;j<Nindv;j++)
                 temp(i,j)=temp(i,j)*mask(j,0);


        return temp.transpose();


}



MatrixXdr  compute_Xz (int num_snp){

         MatrixXdr new_zb= MatrixXdr::Random(Nz,num_snp);
         new_zb = new_zb * sqrt(3);

	 MatrixXdr new_res(Nz, Nindv);         

        if(use_mailman==true)
                multiply_y_post_fast(new_zb,Nz,new_res, false);
        else
                new_res=new_zb*gen;


        MatrixXdr new_resid(Nz, num_snp);
       MatrixXdr zb_scale_sum = new_zb * means;
       new_resid = zb_scale_sum * MatrixXdr::Constant(1,Nindv, 1);


                      /// new zb 
       MatrixXdr temp=new_res - new_resid;

        for (int i=0;i<Nz;i++)
           for(int j=0;j<Nindv;j++)
                 temp(i,j)=temp(i,j)*mask(j,0);




	return temp.transpose();
}








void read_annot (string filename){
         
//	int step_size=Nsnp/Njack;
  //      int step_size_rem=Nsnp%Njack;
        vector<bool> snp_annot;
	//jack_bin.resize(Njack, vector<int>(Nbin,0));	
	
//	cout<<step_size<<endl;

	ifstream inp(filename.c_str());
        if (!inp.is_open()){
                cerr << "Error reading file "<< filename <<endl;
                exit(1);
        }
        string line;
        int j = 0 ;
        int linenum = 0 ;
        int num_parti;
        stringstream check1(line);
        string intermediate;
        vector <string> tokens;
        while(std::getline (inp, line)){
                //linenum ++;
                char c = line[0];
                if (c=='#')
                        continue;
                istringstream ss (line);
                if (line.empty())
                        continue;
                j++;
                //cout<<line<<endl;

                stringstream check1(line);
                string intermediate;
                vector <string> tokens;
                // Tokenizing w.r.t. space ' ' 
                while(getline(check1, intermediate, ' '))
                 {
                      tokens.push_back(intermediate);
                 }
                 if(linenum==0){
                 num_parti=tokens.size();
		 Nbin=num_parti;
                  snp_annot.resize(Nbin,0);	 
          	  jack_bin.resize(Njack, vector<int>(Nbin,0));
	         len.resize(num_parti,0);
                }
                int index_annot=0;
                for(int i = 0; i < tokens.size(); i++){
		      snp_annot[i]=0;
		      if (tokens[i]=="1"){
                            len[i]++;
			    snp_annot[i]=1;
 		      }
                }
		annot_bool.push_back(snp_annot);
        linenum++;
       }

	  if(Nsnp!=linenum){
          cout<<"Number of the rows in bim file and annotation file does not match"<<endl;
        }

	Nsnp=linenum;
	//cout<<"Total number of SNPs : "<<Nsnp<<endl;
	int selected_snps=0;
	for (int i=0;i<num_parti;i++){
                cout<<len[i]<<" SNPs in "<<i<<"-th bin"<<endl;
		selected_snps+=len[i];
        }
	
	cout<<" Number of selected SNPs w.r.t  annot file : " <<selected_snps<<endl;


	 step_size=Nsnp/Njack;
         step_size_rem=Nsnp%Njack;
	cout<<"Number of SNPs per block : "<<step_size<<endl;
      //  cout<<"stepsize : "<<step_size_rem<<endl;
	jack_bin.resize(Njack, vector<int>(Nbin,0));
	int temp;
	for (int i=0;i<Nsnp;i++)
	   for(int j=0;j<Nbin;j++)
		 if (annot_bool[i][j]==1){
			temp=i/step_size;
			if (temp>=Njack)
				temp=Njack-1;
			//cout<<i<<"xxx"<<j<<"xxx"<<temp<<endl;
			jack_bin[temp][j]++;	
		 }
/*
cout<<"jackbin"<<endl;
	for (int i=0;i<Njack;i++){
	   for(int j=0;j<Nbin;j++)
                cout<<jack_bin[i][j]<<" ";
	  cout<<endl;
        }
*/
/*cout<<"annnot"<<endl;
for (int i=0;i<linenum;i++){
  for(int j=0;j<Nbin;j++)
	cout<<annot_bool[i][j]<<" ";
  cout<<endl;
}
*/

}


//vector <int> SNP_annot;

void read_annot_1col (string filename){
	
	
        ifstream ifs(filename.c_str(), ios::in);

        std::string line;
        std::istringstream in;
        
	
	len.resize(Nbin,0);
	 step_size=Nsnp/Njack;
         step_size_rem=Nsnp%Njack;
        cout<<"Number of SNPs per block : "<<step_size<<endl;
	jack_bin.resize(Njack, vector<int>(Nbin,0));
	int i=0;
        while(std::getline(ifs, line)){
                in.clear();
                in.str(line);
                string temp;
                
		in>>temp;        
	        int  cur = atoi(temp.c_str());
		SNP_annot.push_back(cur);
		len[cur-1]++;
	
		int jack_val=i/step_size;
		 if (jack_val==Njack)
                    jack_val--;
		jack_bin[jack_val][SNP_annot[i]-1]++;

               
		 i++;
        }

	if(Nsnp!=i){
	  cout<<"Number of the rows in bim file and annotation file does not match"<<endl;
	}


        cout<<"Total number of SNPs : "<<Nsnp<<endl;
        for (int i=0;i<Nbin;i++){
                cout<<len[i]<<" SNPs in "<<i<<"-th bin"<<endl;
        }

        /*for (int i=0;i<Njack;i++){
           for(int j=0;j<Nbin;j++)
                cout<<jack_bin[i][j]<<" ";
          cout<<endl;
        }*/


}

void read_bim (string filename){
        ifstream inp(filename.c_str());
        if (!inp.is_open()){
                cerr << "Error reading file "<< filename <<endl;
                exit(1);
        }
        string line;
        int j = 0 ;
        int linenum = 0 ;
        while(std::getline (inp, line)){
                linenum ++;
                char c = line[0];
                if (c=='#')
                        continue;
                istringstream ss (line);
                if (line.empty())
                        continue;
                j++;
        }
        Nsnp = j;
        inp.close();
	cout<<"#SNP in bim file "<<Nsnp<<endl;
}





void count_pheno(std::string filename){
        ifstream ifs(filename.c_str(), ios::in);

        std::string line;
        int i=0;
        while(std::getline(ifs, line)){
                i++;
        }
        Nindv=i-1;
}




int  count_fam(std::string filename){
        ifstream ifs(filename.c_str(), ios::in);

        std::string line;
        int i=0;
        while(std::getline(ifs, line)){
                i++;
        }
        return i;
}



//// functions related to reading without mailman

template<typename T>
static std::istream & binary_read(std::istream& stream, T& value){
        return stream.read(reinterpret_cast<char*>(&value), sizeof(T));
}

void set_metadata() {
        wordsize = sizeof(char) * 8;
        unitsize = 2;
        unitsperword = wordsize/unitsize;
        mask2 = 0;
        for (int i = 0 ; i < unitsize; i++)
                mask2 = mask2 |(0x1<<i);
    nrow = Nsnp;
    ncol = ceil(1.0*Nindv/unitsperword);
}


int simulate2_geno_from_random(float p_j){
        float rval = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        float dist_pj[3] = { (1-p_j)*(1-p_j), 2*p_j*(1-p_j), p_j*p_j };
        if(rval < dist_pj[0] )
                return 0;
        else if( rval >= dist_pj[0] && rval < (dist_pj[0]+dist_pj[1]))
                return 1;
        else
                return 2;
}









float get_observed_pj(const unsigned char* line){
        int y[4];
        int observed_sum=0;
        int observed_ct=0;

        for (int k = 0 ;k < ncol ; k++) {
                unsigned char c = line [k];
                y[0] = (c)&mask2;
                y[1] = (c>>2)&mask2;
                y[2] = (c>>4)&mask2;
                y[3] = (c>>6)&mask2;
                int j0 = k * unitsperword;
                int lmax = 4;
                if (k == ncol - 1)  {
                        lmax = Nindv%4;
                        lmax = (lmax==0)?4:lmax;
                }
                for ( int l = 0 ; l < lmax; l++){
                        int j = j0 + l ;
                        // Extract  PLINK coded genotype and convert into 0/1/2
                        // // PLINK coding: 
                        // // 00->0
                        // // 01->missing
                        // // 10->1
                        // // 11->2
                        int val = y[l];
                        val-- ;
                        if(val != 0){
                                val =  (val < 0 ) ? 0 :val ;
                                observed_sum += val;
                                observed_ct ++;
                        }
                }
        }
        return observed_sum*0.5/observed_ct;

}


void read_bed2 (std::istream& ifs,bool allow_missing,int num_snp)  {
         //ifstream ifs (filename.c_str(), ios::in|ios::binary);
        char magic[3];
        set_metadata ();

    gtype =  new unsigned char[ncol];

     if(read_header)
      binary_read(ifs,magic);
 
        int sum=0;

        // Note that the coding of 0 and 2 can get flipped relative to plink because plink uses allele frequency (minor)
        // allele to code a SNP as 0 or 1.
        // This flipping does not matter for results.
        int y[4];

int bin_pointer;

vector<int> pointer_bins;

for(int i=0;i<num_snp;i++){
                global_snp_index++;
                ifs.read (reinterpret_cast<char*>(gtype), ncol*sizeof(unsigned char));
                float p_j = get_observed_pj(gtype);
	 

	   pointer_bins.clear();      
       	  for(int bin_index=0;bin_index<Nbin;bin_index++)
       		if(annot_bool[global_snp_index][bin_index]==1)
			  pointer_bins.push_back(bin_index);
			//bin_pointer=bin_index;

	  for (int k = 0 ;k < ncol ; k++) {
                unsigned char c = gtype [k];
                        // Extract PLINK genotypes
                y[0] = (c)&mask2;
                y[1] = (c>>2)&mask2;
                y[2] = (c>>4)&mask2;
                y[3] = (c>>6)&mask2;
                        int j0 = k * unitsperword;
                        // Handle number of individuals not being a multiple of 4
                        int lmax = 4;
                        if (k == ncol - 1)  {
                                lmax = Nindv%4;
                                lmax = (lmax==0)?4:lmax;
                        }
                        for ( int l = 0 ; l < lmax; l++){
                                int j = j0 + l ;
                                // Extract  PLINK coded genotype and convert into 0/1/2
                                // PLINK coding: 
                                // 00->0
                                // 01->missing
                                // 10->1
                                // 11->2
                                int val = y[l];
                                if(val==1 && !allow_missing){
                                        val = simulate2_geno_from_random(p_j);
                                        val++;
                                        val = (val==1) ? 0 : val;
                                   //val=0;
                                 }
                                val-- ;
                                val =  (val < 0 ) ? 0 :val ;
                                sum += val;

                            for(int bin_index=0;bin_index<pointer_bins.size();bin_index++){
					
				       bin_pointer=pointer_bins[bin_index];
                                      int snp_index;

                                     if(use_mailman==true){
                                         snp_index=allgen_mail[bin_pointer].index;
                                         int horiz_seg_no = snp_index/allgen_mail[bin_pointer].segment_size_hori;
                                         allgen_mail[bin_pointer].p[horiz_seg_no][j] = 3 *allgen_mail[bin_pointer].p[horiz_seg_no][j]  + val;
                                     // computing sum for every snp to compute mean
                                         allgen_mail[bin_pointer].columnsum[snp_index]+=val;

                                      }
                                     else{
                                         snp_index=allgen[bin_pointer].index;
                                         allgen[bin_pointer].gen(snp_index,j)=val;
                                     }


                            }

                    }
        }

    for(int bin_index=0;bin_index<pointer_bins.size();bin_index++){
     		bin_pointer=pointer_bins[bin_index];
	         if(use_mailman==true)
                  allgen_mail[bin_pointer].index++;
               else
                   allgen[bin_pointer].index++;
    }	



}

        sum = 0 ;
        delete[] gtype;
}



void read_bed_1colannot (std::istream& ifs,bool allow_missing,int num_snp)  {
         //ifstream ifs (filename.c_str(), ios::in|ios::binary);
        char magic[3];
        set_metadata ();

    gtype =  new unsigned char[ncol];

     if(read_header)
      binary_read(ifs,magic);

        int sum=0;

        // Note that the coding of 0 and 2 can get flipped relative to plink because plink uses allele frequency (minor)
        // allele to code a SNP as 0 or 1.
        // This flipping does not matter for results.
        int y[4];

int bin_pointer;

//vector<int> pointer_bins;

for(int i=0;i<num_snp;i++){
                global_snp_index++;
                ifs.read (reinterpret_cast<char*>(gtype), ncol*sizeof(unsigned char));
                float p_j = get_observed_pj(gtype);

          for (int k = 0 ;k < ncol ; k++) {
                unsigned char c = gtype [k];
                        // Extract PLINK genotypes
                y[0] = (c)&mask2;
                y[1] = (c>>2)&mask2;
                y[2] = (c>>4)&mask2;
                y[3] = (c>>6)&mask2;
                        int j0 = k * unitsperword;
                        // Handle number of individuals not being a multiple of 4
                        int lmax = 4;
                        if (k == ncol - 1)  {
                                lmax = Nindv%4;
                                lmax = (lmax==0)?4:lmax;
                        }
                        for ( int l = 0 ; l < lmax; l++){
                                int j = j0 + l ;
                                // Extract  PLINK coded genotype and convert into 0/1/2
                                // PLINK coding: 
                                // 00->0
                                // 01->missing
                                // 10->1
                                // 11->2
                                int val = y[l];
                                if(val==1 && !allow_missing){
                                        val = simulate2_geno_from_random(p_j);
                                        val++;
                                        val = (val==1) ? 0 : val;
                                   //val=0;
                                 }
                                val-- ;
                                val =  (val < 0 ) ? 0 :val ;
                                sum += val;
//cout<<"sss"<<endl;
//cout<<global_snp_index<<endl;

                                       bin_pointer=SNP_annot[global_snp_index]-1;
                           
//	cout<<bin_pointer<<endl;
			           int snp_index;
                                     if(use_mailman==true){
                                         snp_index=allgen_mail[bin_pointer].index;
                                         int horiz_seg_no = snp_index/allgen_mail[bin_pointer].segment_size_hori;
                                         allgen_mail[bin_pointer].p[horiz_seg_no][j] = 3 *allgen_mail[bin_pointer].p[horiz_seg_no][j]  + val;
                                     // computing sum for every snp to compute mean
                                         allgen_mail[bin_pointer].columnsum[snp_index]+=val;

                                      }
                                     else{
                                         snp_index=allgen[bin_pointer].index;
                                         allgen[bin_pointer].gen(snp_index,j)=val;
                                     }


                    }
        }

        	bin_pointer=SNP_annot[global_snp_index]-1;
	         if(use_mailman==true)
                  allgen_mail[bin_pointer].index++;
               else
                   allgen[bin_pointer].index++;
}

  sum = 0 ;
  delete[] gtype;



}

MatrixXdr jack_se(MatrixXdr jack){

int nrows=jack.rows();
int ncols=jack.cols();
MatrixXdr sum_row=jack.rowwise().mean();
//cout<<sum_row<<endl;
MatrixXdr SEjack;
SEjack=MatrixXdr::Zero(nrows,1);
double temp_val=0;
for (int i=0;i<nrows;i++){
    for (int j=0;j<ncols;j++){
        temp_val=jack(i,j)-sum_row(i);
        temp_val= temp_val* temp_val;
        SEjack(i,0)+=temp_val;
    }
    SEjack(i,0)=SEjack(i,0)*(Njack-1)/Njack;
    SEjack(i,0)=sqrt(SEjack(i,0));
}

return SEjack;
}

void genotype_stream_pass(string name, int pass_num){


ifstream ifs (name.c_str(), ios::in|ios::binary);
read_header=true;
global_snp_index=-1;

MatrixXdr output;
MatrixXdr vec1;
MatrixXdr w1;
MatrixXdr w2;
MatrixXdr w3;

MatrixXdr  A_trs(Nbin,Nbin);
MatrixXdr b_trk(Nbin,1);
MatrixXdr c_yky(Nbin,1);

MatrixXdr X_l(Nbin+1,Nbin+1);
MatrixXdr Y_r(Nbin+1,1);
MatrixXdr B1;
MatrixXdr B2;
MatrixXdr C1;
MatrixXdr C2;
double trkij;
double yy=(pheno.array() * pheno.array()).sum();

if(both_side_cov==true){
MatrixXdr Wty=covariate.transpose()*pheno; //W^ty
MatrixXdr QWty=Q*Wty;
double temp=(Wty.array() * QWty.array()).sum();
        yy=yy-temp;
}

Nindv_mask=mask.sum();
if(both_side_cov==true)
   NC=Nindv_mask-cov_num;
else
   NC=Nindv_mask;



MatrixXdr herit;


/*MatrixXdr jack;
MatrixXdr point_est;
MatrixXdr enrich_jack;
MatrixXdr enrich_point_est;
*/
jack.resize(Nbin+1,Njack);
point_est.resize(Nbin+1,1);

enrich_jack.resize(Nbin,Njack);
enrich_point_est.resize(Nbin,1);


MatrixXdr h1;
MatrixXdr h2;
MatrixXdr h3;

double trkij_res1;
double trkij_res2;
double trkij_res3;
double tk_res;




for (int jack_index=0;jack_index<Njack;jack_index++){	

	int read_Nsnp=(jack_index<(Njack-1)) ? (step_size) : (step_size+step_size_rem);
	cout<<"Reading block "<<jack_index<<endl;

       if(use_mailman==true){
        for (int i=0;i<Nbin;i++){
	allgen_mail[i].segment_size_hori = floor(log(Nindv)/log(3)) - 2 ;
        allgen_mail[i].Nsegments_hori = ceil(jack_bin[jack_index][i]*1.0/(allgen_mail[i].segment_size_hori*1.0));
        allgen_mail[i].p.resize(allgen_mail[i].Nsegments_hori,std::vector<int>(Nindv));
        //allgen_mail[i].not_O_i.resize(jack_bin[jack_index][i]);
        //allgen_mail[i].not_O_j.resize(Nindv);
	allgen_mail[i].index=0;
	allgen_mail[i].Nsnp=jack_bin[jack_index][i];
	allgen_mail[i].Nindv=Nindv;
       	
	 allgen_mail[i].columnsum.resize(jack_bin[jack_index][i],1);
	  for (int index_temp=0;index_temp<jack_bin[jack_index][i];index_temp++)
		    allgen_mail[i].columnsum[index_temp]=0;

	 }
       }
       else{
	   for (int k=0;k<Nbin;k++){
                allgen[k].gen.resize(jack_bin[jack_index][k],Nindv);
                allgen[k].index=0;
           }
       }

       
	if(use_1col_annot==true)
		read_bed_1colannot(ifs,missing,read_Nsnp);
	else
		read_bed2(ifs,missing,read_Nsnp);
       read_header=false;

     for (int bin_index=0;bin_index<Nbin;bin_index++){
	  int num_snp;
	  if (use_mailman==true)
		num_snp=allgen_mail[bin_index].index;
	  else
		num_snp=allgen[bin_index].index;
	
	  //cout<<"bin number"<<bin_index<<"number of SNPs"<<num_snp<<endl;

	  if(num_snp!=0){
	  stds.resize(num_snp,1);
	  means.resize(num_snp,1);
	//  	   cout<<"bin number"<<bin_index<<"number of SNPs"<<num_snp<<endl;
	  if(use_mailman==true){
		for (int i=0;i<num_snp;i++)
		   means(i,0)=(double)allgen_mail[bin_index].columnsum[i]/Nindv;
    	 }			
          else	  
		means=allgen[bin_index].gen.rowwise().mean();
          

	  for (int i=0;i<num_snp;i++)
	       stds(i,0)=1/sqrt((means(i,0)*(1-(0.5*means(i,0)))));


	   if (use_mailman==true){
	   	g=allgen_mail[bin_index];
                 g.segment_size_hori = floor(log(Nindv)/log(3)) - 2 ;
        	 g.Nsegments_hori = ceil(jack_bin[jack_index][bin_index]*1.0/(g.segment_size_hori*1.0));
        	 g.p.resize(g.Nsegments_hori,std::vector<int>(Nindv));
        	 //g.not_O_i.resize(jack_bin[jack_index][bin_index]);
        	 //g.not_O_j.resize(Nindv);
		initial_var();
	   }
	  else{
		 gen=allgen[bin_index].gen;
		
          }  

	 output=compute_XXz(num_snp);

	   for (int z_index=0;z_index<Nz;z_index++){
		//if(num_snp!=len[bin_index])
                 //XXz.col((bin_index*(Njack+1)*Nz)+(jack_index*Nz)+z_index)=output.col(z_index);
                 if(pass_num==1){
		         XXz.col((bin_index*2*Nz)+Nz+z_index)+=output.col(z_index);   /// save whole sample
		 }
		 else if(num_snp!=len[bin_index])
			 XXz.col((bin_index*2*Nz)+z_index)=XXz.col((bin_index*2*Nz)+Nz+z_index)-output.col(z_index);   /// save corresponding jack contrib

		 if(both_side_cov==true) {
		  vec1=output.col(z_index);
		  w1=covariate.transpose()*vec1;
                  w2=Q*w1;
                  w3=covariate*w2;
		  if(pass_num==1){
		     UXXz.col((bin_index*2*Nz)+Nz+z_index)+=w3;
		     UXXz.col((bin_index*2*Nz)+z_index)+=w3;
		  }
		  else if(num_snp!=len[bin_index])
		     UXXz.col((bin_index*2*Nz)+z_index)=UXXz.col((bin_index*2*Nz)+Nz+z_index)-w3;

		 }

            }

	   if (both_side_cov==true){
	      output=compute_XXUz(num_snp); 
	      for (int z_index=0;z_index<Nz;z_index++){
                if(pass_num==1){
		   XXUz.col((bin_index*2*Nz)+Nz+z_index)+=output.col(z_index);   /// save whole sample
		}
	        else if(num_snp!=len[bin_index])
		    XXUz.col((bin_index*2*Nz)+z_index)=XXUz.col((bin_index*2*Nz)+Nz+z_index)-output.col(z_index);
	      }	 
	  }
		   
	   //compute yXXy
		double temp_yxxy;
	   if(both_side_cov==false)
	   temp_yxxy= compute_yXXy(num_snp);
	   else
	   temp_yxxy= compute_yVXXVy(num_snp);

	  if(pass_num==1){
           yXXy(bin_index,1)+=temp_yxxy;
	  }
	  else if(num_snp!=len[bin_index]){
	     yXXy(bin_index,0)= yXXy(bin_index,1)-temp_yxxy;
	   }

	
	//cout<<num_snp<< "SNPs in bin "<<bin_index<<"of jack "<<jack_index<<endl;   
	//cout<<" Reading and computing bin "<<bin_index <<"  of "<< jack_index<<"-th is finished"<<endl;
	   
	    if(use_mailman==true){
		delete[] sum_op;
        	delete[] partialsums;
       		 delete[] yint_e;
        	delete[] yint_m;
        	for (int i  = 0 ; i < hsegsize; i++)
                	delete[] y_m [i];
        	delete[] y_m;

        	for (int i  = 0 ; i < g.Nindv; i++)
                	delete[] y_e[i];
        	delete[] y_e;

        	std::vector< std::vector<int> >().swap(g.p);
        	//std::vector< std::vector<int> >().swap(g.not_O_j);
        	//std::vector< std::vector<int> >().swap(g.not_O_i);
		std::vector< std::vector<int> >().swap(allgen_mail[bin_index].p);
                //std::vector< std::vector<int> >().swap(allgen_mail[bin_index].not_O_j);
                //std::vector< std::vector<int> >().swap(allgen_mail[bin_index].not_O_i);
        	g.columnsum.clear();
        	g.columnsum2.clear();
        	g.columnmeans.clear();
       		 g.columnmeans2.clear();
		 allgen_mail[bin_index].columnsum.clear();
                allgen_mail[bin_index].columnsum2.clear();
                allgen_mail[bin_index].columnmeans.clear();
                 allgen_mail[bin_index].columnmeans2.clear();
	    }
        }

     }
//cout<<" Reading and computing  of "<< jack_index<<"-th is finished"<<endl;

if(pass_num==2){
   

for(int l=0;l<Nbin;l++)
   if( len[l]==jack_bin[jack_index][l])
        jack_bin[jack_index][l]=0;

for (int i=0;i<Nbin;i++){

	if(both_side_cov==false)	
             b_trk(i,0)=Nindv_mask;

        c_yky(i,0)=yXXy(i,0)/(len[i]-jack_bin[jack_index][i]);
  
       
	if(both_side_cov==true){
	B1=XXz.block(0,(i*2*Nz),Nindv,Nz);
	C1=B1.array()*all_Uzb.array();
        C2=C1.colwise().sum();	
	tk_res=C2.sum();  

        tk_res=tk_res/(len[i]-jack_bin[jack_index][i])/Nz;

        b_trk(i,0)=Nindv_mask-tk_res;
	}

       for (int j=i;j<Nbin;j++){
                B1=XXz.block(0,(i*2*Nz),Nindv,Nz);
                B2=XXz.block(0,(j*2*Nz),Nindv,Nz);
                C1=B1.array()*B2.array();
                C2=C1.colwise().sum();
                trkij=C2.sum();


		if(both_side_cov==true){

			h1=covariate.transpose()*B1;
                        h2=Q*h1;
                        h3=covariate*h2;
			C1=h3.array()*B2.array();
		        C2=C1.colwise().sum();
                        trkij_res1=C2.sum();


			B1=XXUz.block(0,(i*2*Nz),Nindv,Nz);
               	        B2=UXXz.block(0,(j*2*Nz),Nindv,Nz);
                        C1=B1.array()*B2.array();
                        C2=C1.colwise().sum();
                        trkij_res3=C2.sum();

			
			trkij+=trkij_res3-trkij_res1-trkij_res1 ;
			
	     }

                 trkij=trkij/(len[i]-jack_bin[jack_index][i])/(len[j]-jack_bin[jack_index][j])/Nz;
                A_trs(i,j)=trkij;
                A_trs(j,i)=trkij;

        }
  }


X_l<<A_trs,b_trk,b_trk.transpose(),NC;
Y_r<<c_yky,yy;
herit=X_l.colPivHouseholderQr().solve(Y_r);
/*
cout<<"X_l"<<endl<<X_l<<endl;
cout<<"Y_r"<<endl<<Y_r<<endl;
cout<<"herit"<<endl<<herit<<endl;
*/
for(int i=0;i<(Nbin+1);i++)
      jack(i,jack_index)=herit(i,0);


//handle when a jackknife block does not include any SNPs from a bin

for (int bin_index=0;bin_index<Nbin;bin_index++){
         for (int z_index=0;z_index<Nz;z_index++){
                  XXz.col((bin_index*2*Nz)+z_index)=XXz.col((bin_index*2*Nz)+Nz+z_index);
                  if(both_side_cov==true){
                  UXXz.col((bin_index*2*Nz)+z_index)=UXXz.col((bin_index*2*Nz)+Nz+z_index);
                  XXUz.col((bin_index*2*Nz)+z_index)=XXUz.col((bin_index*2*Nz)+Nz+z_index);
                  }
         }
        yXXy(bin_index,0)= yXXy(bin_index,1);
}


} //end if pass_num=2




}//end loop over jack
cout<<" Reading and computing  of all blocks are finished"<<endl;

if(pass_num==1){
  //handle when jackknife block does not include any SNPs from a bin//refill
  for (int bin_index=0;bin_index<Nbin;bin_index++){
	 for (int z_index=0;z_index<Nz;z_index++){
		  XXz.col((bin_index*2*Nz)+z_index)=XXz.col((bin_index*2*Nz)+Nz+z_index);
		  if(both_side_cov==true){
		  UXXz.col((bin_index*2*Nz)+z_index)=UXXz.col((bin_index*2*Nz)+Nz+z_index);
		  XXUz.col((bin_index*2*Nz)+z_index)=XXUz.col((bin_index*2*Nz)+Nz+z_index);  
       		  }
	 }
        yXXy(bin_index,0)= yXXy(bin_index,1);
  }
}

if(pass_num==2){
for (int i=0;i<Nbin;i++){
	c_yky(i,0)=yXXy(i,1)/len[i];
	
	 if(both_side_cov==false)
             b_trk(i,0)=Nindv_mask;

	 if(both_side_cov==true){
        	B1=XXz.block(0,(i*2*Nz)+Nz,Nindv,Nz);
        	C1=B1.array()*all_Uzb.array();
        	C2=C1.colwise().sum();
        	tk_res=C2.sum();
                tk_res=tk_res/len[i]/Nz;
        
                 b_trk(i,0)=Nindv_mask-tk_res;
        }

	for (int j=i;j<Nbin;j++){
                B1=XXz.block(0,(i*2*Nz)+Nz,Nindv,Nz);
                B2=XXz.block(0,(j*2*Nz)+Nz,Nindv,Nz);
                C1=B1.array()*B2.array();
                C2=C1.colwise().sum();
                trkij=C2.sum();
                

                if(both_side_cov==true){
                
                        h1=covariate.transpose()*B1;
                        h2=Q*h1;
                        h3=covariate*h2;
                        C1=h3.array()*B2.array();
                        C2=C1.colwise().sum();
                        trkij_res1=C2.sum();
                        

                        B1=XXUz.block(0,(i*2*Nz)+Nz,Nindv,Nz);
                        B2=UXXz.block(0,(j*2*Nz)+Nz,Nindv,Nz);
                        C1=B1.array()*B2.array();
                        C2=C1.colwise().sum();
                        trkij_res3=C2.sum();
                        

                        trkij+=trkij_res3-trkij_res1-trkij_res1 ;
                        
             }
             
                trkij=trkij/len[i]/len[j]/Nz;
                A_trs(i,j)=trkij;
                A_trs(j,i)=trkij;
                
        }
}     
  

X_l<<A_trs,b_trk,b_trk.transpose(),NC;
Y_r<<c_yky,yy;

herit=X_l.colPivHouseholderQr().solve(Y_r);

cout<<"Xl"<<endl<<X_l<<endl;
cout<<"Yl"<<endl<<Y_r<<endl;
   
for(int i=0;i<(Nbin+1);i++)
    point_est(i,0)=herit(i,0);

}

}





int main(int argc, char const *argv[]){
 


parse_args(argc,argv);
////////////////////////////////////////////
///////////////////////////////////////////
    
    //MAX_ITER =  command_line_opts.max_iterations ; 
        int B = command_line_opts.batchNum;
        k_orig = command_line_opts.num_of_evec ;
        debug = command_line_opts.debugmode ;
        check_accuracy = command_line_opts.getaccuracy;
        var_normalize = false;
        accelerated_em = command_line_opts.accelerated_em;
        k = k_orig + command_line_opts.l;
        k = (int)ceil(k/10.0)*10;
        command_line_opts.l = k - k_orig;
        //p = Nsnp;
        //n = Nindv;
        //bool toStop=false;
       // toStop=true;
        srand((unsigned int) time(0));
        //srand(1);
	//Nz=10;
	Nz=command_line_opts.num_of_evec;
        k=Nz;
         ///clock_t io_end = clock();

	Njack=command_line_opts.jack_number;

////
string filename;
//////////////////////////// Read multi genotypes
string line;
 cov_num;
int num_files=0;
string geno_name=command_line_opts.GENOTYPE_FILE_PATH;

/////////Read bim file to count # SNPs
std::stringstream f1;
f1 << geno_name << ".bim";
read_bim (f1.str());

//////Read annotation files
filename=command_line_opts.Annot_PATH;

if(use_1col_annot==true){
read_annot_1col(filename);
}
else{
read_annot(filename);
}
//filename=command_line_opts.Annot_PATH;

///reading phnotype and save the number of indvs
filename=command_line_opts.PHENOTYPE_FILE_PATH;
count_pheno(filename);

std::stringstream f0;
f0 << geno_name << ".fam";
string name_fam=f0.str();
int fam_lines=count_fam(name_fam);

if (fam_lines!=Nindv)
	exitWithError("# indvs in fam file and pheno file does not match ");

read_pheno2(Nindv,filename);
cout<<"Number of Indvs :"<<Nindv<<endl;
y_sum=pheno.sum();

//read covariate
//std::string covfile="/home/alipazoki/UKBB/kathy_pheno/height.covar";
//:qstd::string covfile="/home/alipazoki/UKBB/maf_ld/sub_indv/10k.bmi.covar";
//bool usee_cov=false;
//if(usee_cov==true){
std::string covfile=command_line_opts.COVARIATE_FILE_PATH;
std::string covname="";
if(covfile!=""){
     use_cov=true;
     cov_num=read_cov(false,Nindv, covfile, covname);
	//cout<<cov_num<<endl;
}
else if(covfile==""){
     cout<<"No Covariate File Specified"<<endl;
     both_side_cov=false;

}
/// regress out cov from phenotypes

if(use_cov==true){
MatrixXdr mat_mask=mask.replicate(1,cov_num);
covariate=covariate.cwiseProduct(mat_mask);

MatrixXdr WtW= covariate.transpose()*covariate;
Q=WtW.inverse(); // Q=(W^tW)^-1

if (both_side_cov==false){
MatrixXdr v1=covariate.transpose()*pheno; //W^ty
MatrixXdr v2=Q*v1;            //QW^ty
MatrixXdr v3=covariate*v2;    //WQW^ty
new_pheno=pheno-v3;
pheno=new_pheno.cwiseProduct(mask);

y_sum=pheno.sum();
y_mean = y_sum/mask.sum();


for(int i=0; i<Nindv; i++){
	if(mask(i,0)!=0)
           pheno(i,0) =pheno(i,0) - y_mean; //center phenotype
}
y_sum=pheno.sum();

}

if (both_side_cov==true){
y_sum=pheno.sum();
y_mean = y_sum/mask.sum();
  for(int i=0; i<Nindv; i++){
       if(mask(i,0)!=0)
           pheno(i,0) =pheno(i,0) - y_mean; //center phenotype
  }
y_sum=pheno.sum();

v1=covariate.transpose()*pheno; //W^ty
v2=Q*v1;            //QW^ty
v3=covariate*v2;    //WQW^ty
new_pheno=pheno-v3;
new_pheno=new_pheno.cwiseProduct(mask);



}
	
}
if(use_cov==false){
y_sum=pheno.sum();
y_mean = y_sum/mask.sum();

for(int i=0; i<Nindv; i++){
       if(mask(i,0)!=0)
           pheno(i,0) =pheno(i,0) - y_mean; //center phenotype
  }
y_sum=pheno.sum();

}
                 
////// normalize phenotype

/*
//bool pheno_norm=false;/
y_sum=pheno.sum();
y_mean = y_sum/mask.sum();

//if(pheno_norm==true){
for(int i=0; i<Nindv; i++){
   if(pheno(i,0)!=0)
      pheno(i,0) =pheno(i,0) - y_mean; //center phenotype
}
y_sum=pheno.sum();

//}

*/






//define random vector z's
//Nz=1;

all_zb= MatrixXdr::Random(Nindv,Nz);
all_zb = all_zb * sqrt(3);

boost::mt19937 seedr;
seedr.seed(std::time(0));
boost::normal_distribution<> dist(0,1);
boost::variate_generator<boost::mt19937&, boost::normal_distribution<> > z_vec(seedr, dist);

for (int i=0;i<Nz;i++)
   for(int j=0;j<Nindv;j++)
      all_zb(j,i)=z_vec();




for (int i=0;i<Nz;i++)
   for(int j=0;j<Nindv;j++)
      all_zb(j,i)=all_zb(j,i)*mask(j,0);


/*
for (int i=0;i<Nindv;i++)
for (int j=0;j<Nz;j++)
        all_zb(i,j)=pheno(i,0);
*/

if(both_side_cov==true){

all_Uzb.resize(Nindv,Nz);
for (int j=0;j<Nz;j++){
 MatrixXdr w1=covariate.transpose()*all_zb.col(j);
 MatrixXdr w2=Q*w1;
 MatrixXdr w3=covariate*w2;
 all_Uzb.col(j)=w3;
}

}


MatrixXdr output;
//define 

//e
//Njack=1;

XXz=MatrixXdr::Zero(Nindv,Nbin*Nz*2);

if(both_side_cov==true){
UXXz=MatrixXdr::Zero(Nindv,Nbin*Nz*2);
XXUz=MatrixXdr::Zero(Nindv,Nbin*Nz*2);
}
yXXy=MatrixXdr::Zero(Nbin,2);

if(use_mailman==true) 
  allgen_mail.resize(Nbin);
else
  allgen.resize(Nbin);
 
int bin_index=0;
///// code for handeling overlapping annotations
std::stringstream f3;
f3 << geno_name << ".bed";
string name=f3.str();
cout<<name<<endl;
ifstream ifs (name.c_str(), ios::in|ios::binary);
read_header=true;
global_snp_index=-1;

if (!ifs.is_open()){
   cerr << "Error reading file "<< name  <<endl;
    exit(1);
}
	

cout<<"Start reading genotypes in blocks"<<endl;
    

genotype_stream_pass(name,1);
genotype_stream_pass(name,2);


double temp_sig=0;
double temp_sum=0;

std::ofstream outfile;
string add_output=command_line_opts.OUTPUT_PATH;
outfile.open(add_output.c_str(), std::ios_base::out);



///compute h^2s

MatrixXdr Sigmas=point_est;

temp_sum=point_est.sum();
for (int j=0;j<Nbin;j++){
        point_est(j,0)=point_est(j,0)/temp_sum;
        temp_sig+=point_est(j,0);
}
point_est(Nbin,0)=temp_sig;

MatrixXdr Sigmas_se=jack_se(jack);

for (int i=0;i<Njack;i++){
   temp_sig=0;
   temp_sum=jack.col(i).sum();
   for (int j=0;j<Nbin;j++){
        jack(j,i)=jack(j,i)/temp_sum;
        temp_sig+=jack(j,i);
   }
   jack(Nbin,i)=temp_sig;
   //cout<<"jack"<<i<<" :"<<temp_sig<<endl;
}
////

//MatrixXdr Sigmas_se=jack_se(jack);
cout<<endl<<"OUTPUT: "<<endl<<"Variances: "<<endl;
outfile<<"OUTPUT: "<<endl<<"Variances: "<<endl;
for (int j=0;j<Nbin;j++){
        cout<<"Sigma^2_"<<j<<": "<<Sigmas(j,0)<<"  SE: "<<Sigmas_se(j,0)<<endl;
        outfile<<"Sigma^2_"<<j<<": "<<Sigmas(j,0)<<" SE: "<<Sigmas_se(j,0)<<endl;
}
cout<<"Sigma^2_e: "<<Sigmas(Nbin,0)<<" SE: "<<Sigmas_se(Nbin,0)<<endl;
outfile<<"Sigma^2_e: "<<Sigmas(Nbin,0)<<" SE: "<<Sigmas_se(Nbin,0)<<endl;



////compute h^2 based on ldsc definition
MatrixXdr her_per_snp;
MatrixXdr her_cat_ldsc;
MatrixXdr point_her_cat_ldsc;;
her_cat_ldsc=MatrixXdr::Zero(Nbin,Njack);
MatrixXdr her_per_snp_inbin(Nbin,1);
point_her_cat_ldsc=MatrixXdr::Zero(Nbin,1);

for (int k=0;k<=Njack;k++){

if(k==Njack){
for(int i=0;i<Nbin;i++){
    her_per_snp_inbin(i,0)=(double)point_est(i,0)/len[i];
}
}
else{
for(int i=0;i<Nbin;i++){
    her_per_snp_inbin(i,0)=(double)jack(i,k)/(len[i]-jack_bin[k][i]);
}

}

her_per_snp=MatrixXdr::Zero(Nsnp,1);

for(int i=0;i<Nsnp;i++){
   for(int j=0;j<Nbin;j++){
      if(annot_bool[i][j]==1)
             her_per_snp(i,0)+=her_per_snp_inbin(j,0);	     	  
   }

   if(k==Njack){

   for(int j=0;j<Nbin;j++)
      if(annot_bool[i][j]==1)
             point_her_cat_ldsc(j,0)+=her_per_snp(i,0);
   
   }
   else
   {

   for(int j=0;j<Nbin;j++)
      if(annot_bool[i][j]==1)
             her_cat_ldsc(j,k)+=her_per_snp(i,0);
    
   }

}
}




MatrixXdr  se_her_cat_ldsc=jack_se(her_cat_ldsc);


cout<<endl<<"h^2's (heritabilities) and e's (enrichments) are computed based on Equation 9 (overlapping setting) in the paper  https://doi.org/10.1038/s41467-020-17576-9:"<<endl;
outfile<<endl<<"h^2's (heritabilities) and e's (enrichments) are computed based on Equation 9 (overlapping setting) in the paper  https://doi.org/10.1038/s41467-020-17576-9:"<<endl;

cout<<endl<<"h^2's: "<<endl;
outfile<<"h^2's: "<<endl;
for (int j=0;j<Nbin;j++){
     cout<<"h^2 of bin "<<j<<" : "<<point_her_cat_ldsc(j,0)<<" SE: "<<se_her_cat_ldsc(j,0)<<endl;
     outfile<<"h^2 of bin "<<j<<" : "<<point_her_cat_ldsc(j,0)<<" SE: "<<se_her_cat_ldsc(j,0)<<endl;
}


///prop of h2 ldsc def

for (int i=0;i<Nbin;i++)
  point_her_cat_ldsc(i,0)=(double)point_her_cat_ldsc(i,0)/point_est(Nbin,0);

for(int i=0;i<Njack;i++)
   for(int j=0;j<Nbin;j++)
	her_cat_ldsc(j,i)=(double)her_cat_ldsc(j,i)/jack(Nbin,i);	  


////print prop of h2 
 se_her_cat_ldsc=jack_se(her_cat_ldsc);


cout<<endl<<"h^2_i/h^2_t: "<<endl;
outfile<<"h^2_i/h^2_t: "<<endl;
for (int j=0;j<Nbin;j++){
     cout<<"h^2/h^2_t of bin "<<j<<" : "<<point_her_cat_ldsc(j,0)<<" SE: "<<se_her_cat_ldsc(j,0)<<endl;
     outfile<<"h^2/h^2_t of bin "<<j<<" : "<<point_her_cat_ldsc(j,0)<<" SE: "<<se_her_cat_ldsc(j,0)<<endl;
}


cout<<endl<<"Enrichments: "<<endl;
outfile<<"Enrichments: "<<endl;
for (int j=0;j<Nbin;j++){
        double snp_por=(double)len[j]/Nsnp;
     cout<<"Enrichment of bin "<<j<<" : "<<point_her_cat_ldsc(j,0)/snp_por<<" SE: "<<se_her_cat_ldsc(j,0)/snp_por<<endl;
     outfile<<"Enrichment of bin "<<j<<" : "<<point_her_cat_ldsc(j,0)/snp_por<<" SE: "<<se_her_cat_ldsc(j,0)/snp_por<<endl;
}



///////////////////end h2 based on ldsc def





///compute enrichment

double per_her;
double per_size;
int total_size=0;

//for (int i=0;i<Nbin;i++)
  // total_size+=len[i];
total_size=Nsnp;

for (int j=0;j<Nbin;j++){
        per_her=point_est(j,0)/point_est(Nbin,0);
        per_size=(double)len[j]/total_size;
        enrich_point_est(j,0)=per_her/per_size;
        //cout<<j<<" "<<per_her<<" "<<total_size<<" "<<len[j]<<" "<<per_size<<" "<<enrich_point_est(j,0)<<endl;
}


for (int i=0;i<Njack;i++){
    per_size=0;
    /*total_size=0;
    for (int j=0;j<Nbin;j++){
        total_size+=(len[j]-jack_bin[i][j]);
    } */

   int num_snp_in_jack=(i<(Njack-1)) ? (step_size) : (step_size+step_size_rem);
   total_size=Nsnp-num_snp_in_jack;
   for (int j=0;j<Nbin;j++){
        per_her=jack(j,i)/jack(Nbin,i);
        per_size=(double)(len[j]-jack_bin[i][j])/total_size;
        enrich_jack(j,i)=per_her/per_size;
        }
}



//compute se if h2 and enirchment

MatrixXdr SEjack;
SEjack=MatrixXdr::Zero(Nbin+1,1);
SEjack=jack_se(jack);


MatrixXdr enrich_SEjack;
enrich_SEjack=MatrixXdr::Zero(Nbin,1);
enrich_SEjack=jack_se(enrich_jack);






cout<<endl<<"h^2's (heritabilities) and e's (enrichments) are computed based on Equations 2-4 (non-overlapping setting) in the paper  https://doi.org/10.1038/s41467-020-17576-9:"<<endl;
outfile<<endl<<"h^2's (heritabilities) and e's (enrichments) are computed based on Equations 2-4 (non-overlapping setting) in the paper  https://doi.org/10.1038/s41467-020-17576-9:"<<endl;

cout<<endl<<"h^2's: "<<endl;
outfile<<"h^2's: "<<endl;
for (int j=0;j<Nbin;j++){
     cout<<"h^2 of bin "<<j<<" : "<<point_est(j,0)<<" SE: "<<SEjack(j,0)<<endl;
     outfile<<"h^2 of bin "<<j<<" : "<<point_est(j,0)<<" SE: "<<SEjack(j,0)<<endl;
}
cout<<"Total h^2 : "<<point_est(Nbin,0)<<" SE: "<<SEjack(Nbin,0)<<endl;
outfile<<"Total h^2 : "<<point_est(Nbin,0)<<" SE: "<<SEjack(Nbin,0)<<endl;

cout<<endl<<"Enrichments: "<<endl;
outfile<<"Enrichments: "<<endl;
for (int j=0;j<Nbin;j++){
     cout<<"Enrichment of bin "<<j<<": "<<enrich_point_est(j,0)<<" SE: "<<enrich_SEjack(j,0)<<endl;
     outfile<<"Enrichment of bin "<<j<<": "<<enrich_point_est(j,0)<<" SE: "<<enrich_SEjack(j,0)<<endl;
}


////////////////////////////////////////////////////////

	return 0;
}
