
#ifndef SPBAU_RECOMMENDER_SVDPP_HPP_
#define SPBAU_RECOMMENDER_SVDPP_HPP_

#include <graphlab.hpp>
#include <graphlab/macros_def.hpp>
#include "pmf.h"

using namespace graphlab;

// SVD++ graph ================================================================
struct vertex_data_svdpp {
  vec pvec; //vector of learned values U,V,T
  float rmse; //root of mean square error
  int num_edges; //number of edges
  float bias; //bias for this user/movie
  vec weight; //weight vector for this user/movie

  //constructor
  vertex_data_svdpp();
  
  void save(graphlab::oarchive& archive) const; 
  void load(graphlab::iarchive& archive); 
};

//constructor
vertex_data_svdpp::vertex_data_svdpp(const vec p = zeros(ac.D), float r = 0, float e = num_edges, float b = 0, 
	float w = zeros(ac.D))
	: pvec(p), rmse(r), num_edges(e), bias(b), weight(w)
{
// 	pvec = zeros(ac.D);
// 	rmse = 0;
// 	num_edges = 0;
// 	bias = 0;
// 	weight = zeros(ac.D);
}

void vertex_data_svdpp::save(graphlab::oarchive& archive) const
{
	////TODO archive << pvec;
	archive << rmse << num_edges << bias; 
	///TODO archive << weight;
}  
   
void vertex_data_svdpp::load(graphlab::iarchive& archive)
{
	//TODO archive >> pvec;
	archive >> rmse >> num_edges >> bias;  
	//TODO archive >> weight;
}

struct edge_data
{
	float  weight;  //observation 
//   float  time; //time of observation (for tensor algorithms)
	edge_data(float w = 0)
		: weight(w)
	{ 
		//time = 0; 
	}
	
	void save(graphlab::oarchive& archive) const
	{  
		archive << weight << time; 
	}  

	void load(graphlab::iarchive& archive)
	{  
		archive >> weight >> time;
	}
};

typedef graphlab::graph<vertex_data_svdpp, edge_data> graph_type_svdpp;
typedef graphlab::types<graph_type_svdpp> gl_types_svdpp;

//=============================================================================

template <class D, class M>
void matrix_to_svdpp_graph(const D &dataset, const M &mask, 
						   graph_type_svdpp &graph)
{
	for (int i = 0; i < dataset.rows(); ++i)
	{
		graph.add_vertex(vertex_data_svdpp(/*randu(ac.D)*0.1*/);
	}
	for (int i = 0; i < dataset.cols(); ++i)
	{
		graph.add_vertex(vertex_data_svdpp(/*randu(ac.D)*0.1*/);
	}
	
	for (int i = 0; i < dataset.rows(); ++i)
	{
		for (int j = 0; j < dataset.cols(); ++j)
		{
			if (mask(i,j) == true)
			{
				graph.add_edge(i, dataset.rows() + j, edge_data(1));
			}
		}
	}
}

//=============================================================================

float itmBiasStep = 5e-3f;
float itmBiasReg = 1e-3f;
float usrBiasStep = 2e-4f;
float usrBiasReg = 5e-3f;
float usrFctrStep = 2e-2f;
float usrFctrReg = 2e-2f;
float itmFctrStep = 3e-3f;
float itmFctrReg = 1e-2f; //gamma7
float itmFctr2Step = 1e-4f;
float itmFctr2Reg = 1e-2f;

// extern advanced_config ac;
// extern problem_setup ps;
double regularization = 15e-3;

template<typename graph_type>
void init_svdpp(graph_type* _g){
  assert(false);
}
template<>
void init_svdpp<graph_type_svdpp>(graph_type_svdpp *_g){
   fprintf(stderr, "SVD++ %d factors\n", ac.D);
   for (int i=0; i<ps.M+ps.N; i++){
       vertex_data_svdpp & data = _g->vertex_data(i);
       data.weight = ac.debug ? ones(ac.D) : randu(ac.D);
   }
}

float predict(const vertex_data_svdpp& user, const vertex_data_svdpp& movie, const edge_data * edge, const vertex_data * nothing, float rating, float & prediction){
      assert(nothing == NULL);

      //\hat(r_ui) = \mu + 
      prediction = ps.globalMean[0];
                 // + b_u  +    b_i +
      prediction += user.bias + movie.bias;
                 // + q_i^T   *(p_u      +sqrt(|N(u)|)\sum y_j)
      prediction += dot_prod(movie.pvec,(user.pvec+user.weight));
      prediction = std::min((double)prediction, (double)ac.maxval);
      prediction = std::max((double)prediction, (double)ac.minval);
      float err = rating - prediction;
      assert(!std::isnan(err));
      return err*err; 
}

void predict_missing_value(const vertex_data_svdpp&data, const vertex_data_svdpp& pdata, edge_data& edge, double & sq_err, int&e, int i){
    float prediction = 0;
    predict(data, pdata, &edge, NULL, edge.weight, prediction);
    e++;
}
 

//calculate RMSE. This function is called only before and after grahplab is run.
//during run, agg_rmse_by_movie is called 0 which is much lighter function (only aggregate sums of squares)
double calc_svd_rmse(const graph_type_svdpp * _g, bool test, double & res){

     graph_type_svdpp * g = (graph_type_svdpp*)ps.g<graph_type_svdpp>(TRAINING);

     if (test && ps.Le == 0)
       return NAN;
      
     
     res = 0;
     double sqErr =0;
     int nCases = 0;

     for (int i=0; i< ps.M; i++){
       vertex_data_svdpp & usr = (vertex_data_svdpp&)g->vertex_data(i);
       int n = usr.num_edges; //+1.0 ? //regularization
       usr.weight = zeros(ac.D);
       foreach(edge_id_t oedgeid, g->out_edge_ids(i)) {
         vertex_data_svdpp & movie = (vertex_data_svdpp&)g->vertex_data(g->target(oedgeid)); 
	 usr.weight += movie.weight;
       }
       float usrnorm = float(1.0/sqrt(n));
       usr.weight *= usrnorm;

       foreach(edge_id_t oedgeid, _g->out_edge_ids(i)){
         const edge_data & item = _g->edge_data(oedgeid);
         const vertex_data_svdpp & movie = g->vertex_data(_g->target(oedgeid)); 
         float estScore;
         sqErr += predict(usr, movie, NULL, NULL, item.weight, estScore);
         nCases++;
       }
   }
   res = sqErr;
   assert(nCases == (test?ps.Le:ps.L));
   return sqrt(sqErr/(double)nCases);
}


void svd_post_iter(){
  printf("Entering last iter with %d\n", ps.iiter);

  double res,res2;
  double rmse = agg_rmse_by_user<graph_type_svdpp, vertex_data_svdpp>(res);
  printf("%g) Iter %s %d, TRAIN RMSE=%0.4f VALIDATION RMSE=%0.4f.\n", ps.gt.current_time(), "SVD", ps.iiter,  rmse, calc_svd_rmse(ps.g<graph_type_svdpp>(VALIDATION), true, res2));

  itmFctrStep *= ac.svdpp_step_dec;
  itmFctr2Step *= ac.svdpp_step_dec;
  usrFctrStep *= ac.svdpp_step_dec;
  itmBiasStep *= ac.svdpp_step_dec;
  usrBiasStep *= ac.svdpp_step_dec;

  ps.iiter++;
}


void svd_plus_plus_update_function(gl_types::iscope &scope, 
			 gl_types::icallback &scheduler) {
   assert(false); //mode not supported
}
void svd_plus_plus_update_function(gl_types_mult_edge::iscope &scope, 
			 gl_types_mult_edge::icallback &scheduler) {
   assert(false); //mode not supported
} 
void svd_plus_plus_update_function(gl_types_mcmc::iscope &scope, 
			 gl_types_mcmc::icallback &scheduler) {
   assert(false); //mode not supported
} 
 
/***
 * UPDATE FUNCTION
 */
void svd_plus_plus_update_function(gl_types_svdpp::iscope &scope, 
			 gl_types_svdpp::icallback &scheduler) {
    
  /* GET current vertex data */
  vertex_data_svdpp& user = scope.vertex_data();
 
  
  /* print statistics */
  if (ac.debug&& (scope.vertex() == 0 || ((int)scope.vertex() == ps.M-1) || ((int)scope.vertex() == ps.M) || ((int)scope.vertex() == ps.M+ps.N-1))){
    printf("SVDPP: entering %s node  %u \n", (((int)scope.vertex() < ps.M) ? "movie":"user"), (int)scope.vertex());   
    debug_print_vec((((int)scope.vertex() < ps.M) ? "V " : "U") , user.pvec, ac.D);
  }

  assert((int)scope.vertex() < ps.M+ps.N);

  user.rmse = 0;

  if (user.num_edges == 0){
    return; //if this user/movie have no ratings do nothing
  }


  gl_types_svdpp::edge_list outs = scope.out_edge_ids();
  gl_types_svdpp::edge_list ins = scope.in_edge_ids();
  timer t;

  t.start(); 
  //USER NODES    
  if ((int)scope.vertex() < ps.M){


    user.weight = zeros(ac.D);
    
    foreach(graphlab::edge_id_t oedgeid, outs) {
      vertex_data_svdpp  & movie = scope.neighbor_vertex_data(scope.target(oedgeid)); 
      //sum_{j \in N(u)} y_j 
      user.weight += movie.weight; 
            
    }
  
   // sqrt(|N(u)|) 
   float usrNorm = float(1.0/sqrt(user.num_edges));
   //sqrt(|N(u)| * sum_j y_j
   user.weight *= usrNorm;

   vec step = zeros(ac.D);
 
   // main algorithm, see Koren's paper, just below below equation (16)
   foreach(graphlab::edge_id_t oedgeid, outs) {
      edge_data & edge = scope.edge_data(oedgeid);
      vertex_data_svdpp  & movie = scope.neighbor_vertex_data(scope.target(oedgeid));
      float estScore;
      user.rmse += predict(user, movie, NULL, NULL, edge.weight, estScore); 
      // e_ui = r_ui - \hat{r_ui}
      float err = edge.weight - estScore;
      assert(!std::isnan(user.rmse));
      vec itmFctr = movie.pvec;
      vec usrFactor = user.pvec;
   
      //q_i = q_i + gamma2     *(e_ui*(p_u      +  sqrt(N(U))\sum_j y_j) - gamma7    *q_i)
      movie.pvec += itmFctrStep*(err*(usrFactor +  user.weight)             - itmFctrReg*itmFctr);
      //p_u = p_u + gamma2    *(e_ui*q_i   -gamma7     *p_u)
      user.pvec += usrFctrStep*(err *itmFctr-usrFctrReg*usrFactor);
      assert(!std::isnan(user.pvec[0]));
      step += err*itmFctr;

      //b_i = b_i + gamma1*(e_ui - gmma6 * b_i) 
      movie.bias += itmBiasStep*(err-itmBiasReg*movie.bias);
      //b_u = b_u + gamma1*(e_ui - gamma6 * b_u)
      user.bias += usrBiasStep*(err-usrBiasReg*user.bias);
   }

   step *= float(itmFctr2Step*usrNorm);

   //gamma7 
   double mult = itmFctr2Step*itmFctr2Reg;
   foreach(graphlab::edge_id_t oedgeid, outs){
      vertex_data_svdpp  & movie = scope.neighbor_vertex_data(scope.target(oedgeid));
      //y_j = y_j  +   gamma2*sqrt|N(u)| * q_i - gamma7 * y_j
      movie.weight +=  step                    -  mult  * movie.weight;
   }


   ps.counter[EDGE_TRAVERSAL] += t.current_time();

   if (scope.vertex() == (uint)(ps.M-1))
  	svd_post_iter();
}

}


//=============================================================================


template <class D, class M, class S, class A>
class svdpp_algo_t : public collaborative_filtering_algorithm_t<D, M, S, A>
{
public:
	svdpp_algo_t()
		:collaborative_filtering_algorithm_t("k-NN-GroupLens")
	{}
	virtual void operator()(D &algo_prediction, const D &data, const M &mask, 
							S &users_similarity, 
							const A &avg_users_rating, 
							const A &avg_products_rating)
	{
		gl_types_svdpp::core glcore;
		graph_type_svdpp &g = glcore.graph();
		//add_tasks<gl_types_svdpp::core>(glcore):
		std::vector<vertex_id_t> um;
		int start = 0;
		int end = data.rows();//ps.M;	// number of users
		for (int i=start; i< end; i++)
			um.push_back(i);
		glcore.add_tasks(um, svd_plus_plus_update_function, 1);
	}
};

#endif	// SPBAU_RECOMMENDER_SVDPP_HPP_