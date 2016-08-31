#ifndef NDTFEATURE_G2OGRAPHMAKER_22072016
#define NDTFEATURE_G2OGRAPHMAKER_22072016

#include "g2o/types/slam2d/vertex_se2.h"
#include "g2o/types/slam2d/vertex_point_xy.h"
#include "g2o/types/slam2d/edge_se2.h"
#include "g2o/types/slam2d/edge_se2_pointxy.h"
#include "g2o/types/slam2d/parameter_se2_offset.h"
#include "g2o/types/slam2d/edge_se2_prior.h"
#include "g2o/types/slam2d/edge_se2_link.h"
// #include "types_tutorial_slam2d.h"


#include "g2o/core/sparse_optimizer.h"
#include "g2o/core/block_solver.h"
#include "g2o/core/factory.h"
#include "g2o/core/optimization_algorithm_factory.h"
#include "g2o/core/optimization_algorithm_gauss_newton.h"
#include "g2o/solvers/csparse/linear_solver_csparse.h"

// #include "ndt_feature/ndt_feature_graph.h"
// #include "ndt_feature/utils.h"

#include "Eigen/Core"

#include "bettergraph/PseudoGraph.hpp"
#include "vodigrex/linefollower/SimpleNode.hpp"

#include "das/AssociationInterface.hpp"
#include "covariance.hpp"

namespace ndt_feature {
	
	
	/** 
	 * @brief Class holding a g2o graph optimization
	 * When adding stuuf inside add all element and then call make graph. When giving the "from" "toward" for the edges, take in account than first are all the robot poses and then landmark poses then the prior poses. Thus when linking to landmark i send "nd_of_robot_poses + i". This is not done automoatically to allow for a landmark to landmark link
	 * 
	 */
	
	
	class G2OGraphMarker{
	public :
		typedef g2o::BlockSolver< g2o::BlockSolverTraits<-1, -1> >  SlamBlockSolver;
		typedef g2o::LinearSolverCSparse<SlamBlockSolver::PoseMatrixType> SlamLinearSolver;
	protected:
		g2o::SparseOptimizer _optimizer;
		SlamLinearSolver* _linearSolver;
		SlamBlockSolver* _blockSolver;
		g2o::OptimizationAlgorithmGaussNewton* _solver;
		g2o::SE2 _sensorOffsetTransf;
		g2o::ParameterSE2Offset* _sensorOffset;
		Eigen::IOFormat cleanFmt;
		
		std::vector<g2o::SE2> _robot_positions;
		std::vector<g2o::SE2> _landmark_positions;
		std::vector<g2o::SE2> _prior_landmark_positions;
		std::vector<std::tuple<g2o::SE2, int, int> > _odometry;
		std::vector<std::tuple<g2o::SE2, int, int> > _observation_real_landmarks;
		std::vector<std::tuple<g2o::SE2, int, int> > _edges_prior;
		std::vector<std::tuple<g2o::SE2, int, int> > _link_in_between_maps;
		
		Eigen::Vector2d _transNoise;
		double _rotNoise;
		Eigen::Vector2d _landmarkNoise;
		Eigen::Vector2d _priorNoise;
		Eigen::Vector2d _linkNoise;
		
		double _scale_prior_to_landmark;
		
	public:
		// :( I hate pointers
		G2OGraphMarker(const g2o::SE2& sensoffset, 
						const Eigen::Vector2d& tn, 
						double rn,
						const Eigen::Vector2d& ln,
						const Eigen::Vector2d& pn,
						const Eigen::Vector2d& linkn
  					) : _sensorOffsetTransf(sensoffset), _transNoise(tn), _rotNoise(rn), _landmarkNoise(ln), _priorNoise(pn), _linkNoise(linkn){
			
			_linearSolver = new SlamLinearSolver();
			_linearSolver->setBlockOrdering(false);
			_blockSolver = new SlamBlockSolver(_linearSolver);
			_solver = new g2o::OptimizationAlgorithmGaussNewton(_blockSolver);
			_linearSolver->setBlockOrdering(false);
			_optimizer.setAlgorithm(_solver);

			// add the parameter representing the sensor offset ATTENTION was ist das ?
			_sensorOffset = new g2o::ParameterSE2Offset;
			_sensorOffset->setOffset(_sensorOffsetTransf);
			_sensorOffset->setId(0);
			_optimizer.addParameter(_sensorOffset);
			
			
		};
		
		void save(const std::string& file){
			_optimizer.save(file.c_str());
		}
		
		void scalePrior(){
			auto prior_copy = _prior_landmark_positions;
			_prior_landmark_positions.clear();
			auto it = prior_copy.begin();
			for(it ; it != prior_copy.end() ; ++it){
				Eigen::Vector3d se2_tmp = it->toVector();
				se2_tmp(0) = se2_tmp(0) * _scale_prior_to_landmark;
				se2_tmp(1) = se2_tmp(1) * _scale_prior_to_landmark;
				se2_tmp(2) = se2_tmp(2) * _scale_prior_to_landmark;
				_prior_landmark_positions.push_back(se2_tmp);
			}
			
			auto prior_edge_copy = _edges_prior;
			_edges_prior.clear();
			auto it_edge = prior_edge_copy.begin();
			for(it_edge ; it_edge != prior_edge_copy.end() ; ++it_edge){
				std::cout << "Pushing elements" << std::endl;
				auto element = *it_edge;
				Eigen::Vector3d vec = std::get<0>(element).toVector();
				vec(0) = vec(0) * _scale_prior_to_landmark;
				vec(1) = vec(1) * _scale_prior_to_landmark;
				vec(2) = vec(2) * _scale_prior_to_landmark;
				std::tuple<g2o::SE2, int, int> tup(g2o::SE2(vec(0), vec(1), vec(2)), std::get<1>(element), std::get<2>(element));
				
				_edges_prior.push_back(tup);
			}
			
		}
		
		void scaleVectorPoint(std::vector<cv::Point2f>& to_scale){
			auto to_scale_copy = to_scale;
			to_scale.clear();
			auto it = to_scale_copy.begin();
			for(it ; it != to_scale_copy.end() ; ++it){
				cv::Point2f p;
				p.x = it->x * _scale_prior_to_landmark;
				p.y = it->y * _scale_prior_to_landmark;
				to_scale.push_back(p);
			}
			
		}
		
		void setScalePriorToLandmarks(double p){_scale_prior_to_landmark = p;}
		void setTranslationNoise(const Eigen::Vector2d& tn){_transNoise = tn;};
		void setRotationNoise(double rn){_rotNoise = rn;}
		void setLandmarkNoise(const Eigen::Vector2d& ln){_landmarkNoise = ln;}
		void setPriorNoise(const Eigen::Vector2d& pn){_priorNoise = pn;}
		void setLinkNoise(const Eigen::Vector2d& ln){_linkNoise = ln;}
		const Eigen::Vector2d& getTranslationNoise() const {return _transNoise;}
		Eigen::Vector2d getTranslationNoise() {return _transNoise;}
		const Eigen::Vector2d& getLandmarkNoise() const {return _landmarkNoise;}
		Eigen::Vector2d getLandmarkNoise() {return _landmarkNoise;}
		const Eigen::Vector2d& getPriorNoise() const {return _priorNoise;}
		Eigen::Vector2d getPriorNoise() {return _priorNoise;}
		const Eigen::Vector2d& getLinkNoise() const {return _linkNoise;}
		Eigen::Vector2d getLinkNoise() {return _linkNoise;}
		double getRotationNoise(){return _rotNoise;}
		
		g2o::OptimizableGraph::Vertex* getVertex(int idx){_optimizer.vertex(idx);}
		
		std::vector<g2o::SE2>& getRobotPositions(){return _robot_positions;}
		const std::vector<g2o::SE2>& getRobotPositions() const {return _robot_positions;}		
		
		void addRobotPoseAndOdometry(std::ifstream& in){
			std::string word;
			in >> word;
			double garbage;
			in >> garbage;
			in >> garbage;
			in >> garbage;
			in >> garbage;
			while(in >> word){
				if(word == "VERTEX_SE2"){
					double x, y, theta;
					in >> garbage;
					in >> x;
					in >> y;
					in >> theta;
					g2o::SE2 se2(x, y, theta);
					_robot_positions.push_back(se2);
				}
				else{
					double x, y, theta;
					int from, toward;
					in >> from;
					in >> toward;
					in >> x;
					in >> y;
					in >> theta;
					
					//TEST
					//Wrong because you need composition...
// 					double x_tmp = _robot_positions[toward].toVector()(0) - _robot_positions[from].toVector()(0) ;
// 					double y_tmp = _robot_positions[toward].toVector()(1) - _robot_positions[from].toVector()(1) ;
// 					double angle_tmp = _robot_positions[toward].toVector()(2) - _robot_positions[from].toVector()(2);
// 					
// 					
// 					Eigen::Vector2d real_obs ;
// 					real_obs << _robot_positions[toward].toVector()(0), _robot_positions[toward].toVector()(1);
// 					Eigen::Vector2d observation;
// 					//Projecting real_obs into robot coordinate frame
// 					Eigen::Vector2d trueObservation = _robot_positions[from].inverse() * real_obs;
// 					observation = trueObservation;
// 					
// 					
// 					
// 					std::cout << "VEC FROM RBOT " << _robot_positions[from].toVector() << std::endl;
// 					std::cout << "VEC TOWARD RBOT " << _robot_positions[toward].toVector() << std::endl;
// 					std::cout << "T " << x << " " << y << " " << theta <<std::endl;
// 					std::cout << "T made up" << observation(0) << " " << observation(1) << " " << angle_tmp <<std::endl;
					g2o::SE2 se2(x, y, theta);
// 					g2o::SE2 se2(observation(0), observation(1), angle_tmp);
					std::tuple<g2o::SE2, int, int> tup(se2, from, toward);
					_odometry.push_back(tup);
					in >> garbage;
					in >> garbage;
					in >> garbage;
					in >> garbage;
					in >> garbage;
					in >> garbage;
				}
			}
		}
		
		
		void addLandmarkAndObservation(std::ifstream& in){
			std::string word;
			in >> word;
			double garbage;
			in >> garbage;
			in >> garbage;
			in >> garbage;
			in >> garbage;
			while(in >> word){
				if(word == "VERTEX_SE2"){
					double x, y, theta;
					in >> garbage;
					in >> x;
					in >> y;
					
					std::cout << " position " << x << " " << y << std::endl;
// 					in >> theta;
					g2o::SE2 se2(x, y, 0);
					_landmark_positions.push_back(se2);
				}
				else if(word == "EDGE_SE2"){
					int from, toward;
					in >> from;
					in >> toward;
					assert(from >= 0);
// 					assert(toward >= 6);
// 					assert(from <= 5);
// 					assert(toward <= 33);
					
					int toward_access = toward - _robot_positions.size();
					assert(toward_access >= 0);
					
					std::cout << "Edge : " << from << " -> " << toward << std::endl;
					
					//Calculate observation
					
					std::cout << "VEC " << _landmark_positions[toward_access].toVector() << std::endl;
					std::cout << "VEC RBOT " << _robot_positions[from].toVector() << std::endl;
					
// 					double x_tmp = _landmark_positions[toward_access].toVector()(0) - _robot_positions[from].toVector()(0) ;
// 					double y_tmp = _landmark_positions[toward_access].toVector()(1) - _robot_positions[from].toVector()(1) ;
					double angle_tmp = _robot_positions[toward].toVector()(2) - _robot_positions[from].toVector()(2);
					
					
					Eigen::Vector2d real_obs ;
					real_obs << _landmark_positions[toward_access].toVector()(0), _landmark_positions[toward_access].toVector()(1);
					Eigen::Vector2d observation;
					//Projecting real_obs into robot coordinate frame
					Eigen::Vector2d trueObservation = _robot_positions[from].inverse() * real_obs;
					observation = trueObservation;

					std::cout << "G2O OBSERVATION " << observation << std::endl;
					
					g2o::SE2 se2(observation(0), observation(1), angle_tmp);

					std::cout << "G2O OBSERVATION " << se2.toVector() << std::endl;
					
					std::tuple<g2o::SE2, int, int> tup(se2, from, toward);
					_observation_real_landmarks.push_back(tup);
// 					in >> garbage;
// 					in >> garbage;
// 					in >> garbage;
// 					in >> garbage;
// 					in >> garbage;
// 					in >> garbage;
				}
				else{
					throw std::runtime_error("Miss read file for landmarks");
				}
			}
		}
		
		void addRobotPose(const g2o::SE2& se2){
			_robot_positions.push_back(se2);
		}
		
		void addRobotPose(const Eigen::Vector3d& rob){
			g2o::SE2 se2(rob(0), rob(1), rob(2));
			_robot_positions.push_back(se2);
		}
		void addRobotPose(double x, double y, double theta){
			Eigen::Vector3d robot1;
			robot1 << x, y, theta;
			_robot_positions.push_back(robot1);
		}
		
		void addLandmarkPose(const g2o::SE2& se2){
			_landmark_positions.push_back(se2);
		}
		void addLandmarkPose(const Eigen::Vector3d& lan){
			g2o::SE2 se2(lan(0), lan(1), lan(2));
			_landmark_positions.push_back(se2);
		}
		void addLandmarkPose(double x, double y, double theta){
			Eigen::Vector3d lan;
			lan << x, y, theta;
			_landmark_positions.push_back(lan);
		}
		
		void addPriorLandmarkPose(const g2o::SE2& se2){
			_prior_landmark_positions.push_back(se2);
		}
		void addPriorLandmarkPose(const Eigen::Vector3d& lan){
			g2o::SE2 se2(lan(0), lan(1), lan(2));
			addPriorLandmarkPose(se2);
		}
		void addPriorLandmarkPose(double x, double y, double theta){
			Eigen::Vector3d lan;
			lan << x, y, theta;
			_prior_landmark_positions.push_back(lan);
		}
		
		
		void addOdometry(const std::tuple<g2o::SE2, int, int>& odo){
			_odometry.push_back(odo);
		}
		void addOdometry(g2o::SE2 observ, int from, int toward){
			std::tuple<g2o::SE2, int, int> obs1(observ, from, toward);
			addOdometry(obs1);
		}
		void addOdometry(double x, double y, double theta, int from, int toward){
			g2o::SE2 se2(x, y, theta);
			addOdometry(se2, from, toward);
			
		}
		
		void addLandmarkObservation(const std::tuple<g2o::SE2, int, int>& obs){
			_observation_real_landmarks.push_back(obs);
		}
		void addLandmarkObservation(g2o::SE2 observ, int from, int toward){
			std::tuple<g2o::SE2, int, int> obs1(observ, from, toward);
			addLandmarkObservation(obs1);
		}
		void addLandmarkObservation(double x, double y, double theta, int from, int toward){
			g2o::SE2 se2(x, y, theta);
			addLandmarkObservation(se2, from, toward);
			
		}
		
		void addEdgePrior(const std::tuple<g2o::SE2, int, int>& edg){
			_edges_prior.push_back(edg);
		}
		void addEdgePrior(g2o::SE2 observ, int from, int toward){
			std::tuple<g2o::SE2, int, int> obs1(observ, from, toward);
			addEdgePrior(obs1);
		}
		void addEdgePrior(double x, double y, double theta, int from, int toward){
			g2o::SE2 se2(x, y, theta);
			addEdgePrior(se2, from, toward);
			
		}
		
		void addLinkBetweenMaps(const std::tuple<g2o::SE2, int, int>& lnk){
			_link_in_between_maps.push_back(lnk);
		}
		void addLinkBetweenMaps(g2o::SE2 observ, int from, int toward){
			std::tuple<g2o::SE2, int, int> obs1(observ, from, toward);
			addLinkBetweenMaps(obs1);
		}
		void addLinkBetweenMaps(double x, double y, double theta, int from, int toward){
			g2o::SE2 se2(x, y, theta);
			addLinkBetweenMaps(se2, from, toward);
			
		}
		
		
		void addLinkBetweenMaps(std::ifstream& in){
			
			AASS::das::AssociationInterface asso;
			double scale;
			asso.fromFile(in, scale);
			addLinkBetweenMaps(asso, scale);
			
		}
		
		void addLinkBetweenMaps(const AASS::das::AssociationInterface& assoInter, double scale){

			auto getIdenticalPoint =[this](const std::vector<cv::Point2f>& model_points, std::deque< int >& model_same) -> void{
				for(size_t i = 0; i < model_points.size(); ++i){
					bool more_than_one = false;
					for(size_t j = 0 ; j < this->_landmark_positions.size(); ++j ){
						auto translation = this->_landmark_positions[j].translation();
						
						if(translation(0) == model_points[i].x && translation(1) == model_points[i].y){
							model_same.push_back(j);
							if(more_than_one == true) throw std::runtime_error("More than one point linked to key point");
							more_than_one = true;
						}
						
					}
				}
				
			};
			
			auto getIdenticalLandmark =[this](const cv::Point2f& model_points, int& model_same) -> void{
				bool more_than_one = false;
				for(size_t j = 0 ; j < this->_landmark_positions.size(); ++j ){
					Eigen::Vector2d translation = this->_landmark_positions[j].translation();
					Eigen::Vector2d vectrans;
					vectrans << std::floor( (translation(0) * 10) + 0.5 ) /10, std::floor( (translation(1) * 10) + 0.5 ) /10;
					
					std::cout << "T : " << translation << std::endl;
					std::cout << "Point : " << model_points << std::endl;
					Eigen::Vector2d vec;
					vec << std::floor( (model_points.x * 10) + 0.5 ) /10, std::floor( (model_points.y * 10) + 0.5 ) /10;
					
					std::cout <<  translation(0) << " == " << vec(0) << std::endl;
					if(vectrans(0) == vec(0)){
						std::cout << "FIRST GOOD" << std::endl;
						
						std::cout <<  translation(1) << " == " << vec(1) << std::endl;
						if(vectrans(1) == vec(1)){
							std::cout << "FOUND" << std::endl;
							model_same = j;
							if(more_than_one == true) throw std::runtime_error("More than one point linked to key point");
							more_than_one = true;
						}
						else{
							std::cout << "Not the same " << translation(1) - vec(1) << std::endl;
						}
					}
					
				}
				
			};
			
			auto getIdenticalPriorLandmark =[this](const cv::Point2f& model_points, int& model_same) -> void{
				bool more_than_one = false;
				std::cout << std::endl << "-------------------------------------> PRIOR" << std::endl;
				for(size_t j = 0 ; j < this->_prior_landmark_positions.size(); ++j ){
					auto translation = this->_prior_landmark_positions[j].translation();
					Eigen::Vector2d vectrans;
					vectrans << std::floor( (translation(0) * 10) + 0.5 ) /10, std::floor( (translation(1) * 10) + 0.5 ) /10;
					
					std::cout << "T : " << translation << std::endl;
					std::cout << "Point : " << model_points << std::endl;
					Eigen::Vector2d vec;
					vec << std::floor( (model_points.x * 10) + 0.5 ) /10, std::floor( (model_points.y * 10) + 0.5 ) /10;
					
					
					if(vectrans(0) == vec(0) && vectrans(1) == vec(1)){
						model_same = j;
						if(more_than_one == true) throw std::runtime_error("More than one point linked to key point");
						more_than_one = true;
					}
					
				}
				
			};
			
			auto scalePriorLink = [this](std::vector<std::pair < cv::Point2f, cv::Point2f> >& to_scale){
				auto to_scale_copy = to_scale;
				to_scale.clear();
				auto it = to_scale_copy.begin();
				for(it ; it != to_scale_copy.end() ; ++it){
					auto p = *it;
					p.first.x = it->first.x * this->_scale_prior_to_landmark;
					p.first.y = it->first.y * this->_scale_prior_to_landmark;
					to_scale.push_back(p);
				}
				
			};
			
			auto all_links = assoInter.getAssociations();
			
			scalePriorLink(all_links);
			
// 			auto model_points = assoInter.getKeypointModel();
// 			auto data_points = assoInter.getKeypointData();
// 			
// 			std::deque< int > model_same;
// 			std::deque< int > data_same;
// 			
// 			getIdenticalPoint(model_points, model_same);
// 			getIdenticalPoint(data_points, data_same);
			
			//Find all associated vertex of model and data_points
			
			int num_robot_poses = _robot_positions.size();
			int num_landmark_poses = _landmark_positions.size();
			
			g2o::SE2 se2(0, 0, 0);
			
			//Create all the links
			for(size_t i = 0 ; i < all_links.size() ; ++i){
				int idx = -1;
				std::cout << "Searching " << all_links[i].second << " landmark " << std::endl;
				getIdenticalLandmark(all_links[i].second, idx);
				int idx_prior = -1;
				getIdenticalPriorLandmark(all_links[i].first, idx_prior);
				
				if(idx == -1) throw std::runtime_error("index for landmark not found");
				if(idx_prior == -1) throw std::runtime_error("index for prior landmark not found");
				
				const std::tuple<g2o::SE2, int, int> lnk(se2, num_robot_poses + idx, num_robot_poses + num_landmark_poses + idx_prior);
				addLinkBetweenMaps(lnk);
				
			}
			
		}
		
		void addAllPriors(const bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>& graph){
			
			std::pair< bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>::VertexIterator, bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>::VertexIterator > vp;
			
			std::deque<bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>::Vertex> vec_deque;
			
			for (vp = boost::vertices(graph); vp.first != vp.second; ++vp.first) {
				bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>::Vertex v = *vp.first;
				//ATTENTION Magic number
				addPriorLandmarkPose(graph[v].getX(), graph[v].getY(), 0);
				std::cout << "Prior Landmark : " << graph[v].getX() << " " << graph[v].getY() << std::endl;
				vec_deque.push_back(v);
			}
			
			int previous_node_number = _robot_positions.size() + _landmark_positions.size();
			int count = 0;
			
			for (vp = boost::vertices(graph); vp.first != vp.second; ++vp.first) {
				bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>::Vertex v = *vp.first;
				bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>::EdgeIterator out_i, out_end;
				bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>::Edge e;
				
				for (boost::tie(out_i, out_end) = boost::out_edges(v, (graph)); 
					out_i != out_end; ++out_i) {
					e = *out_i;
					bettergraph::PseudoGraph<AASS::vodigrex::SimpleNode, AASS::vodigrex::SimpleEdge>::Vertex targ = boost::target(e, (graph));
				
					int idx = -1;
					for(size_t ii = count +1 ; ii < vec_deque.size() ; ++ii){
						if(targ == vec_deque[ii]){
							idx = ii;
						}
					}
					if(idx == -1){
						//SKIP
					}
					else{
						int x_diff = graph[targ].getX() - graph[v].getX();
						int y_diff = graph[targ].getY() - graph[v].getY();
						
// 						x_diff = std::abs(x_diff);
// 						y_diff = std::abs(y_diff);
						
						g2o::SE2 se2(x_diff, y_diff, 0);
						std::tuple<g2o::SE2, int, int> tup(se2, previous_node_number + count, previous_node_number + idx);
						addEdgePrior(tup);
					}
				
				}
				
				++count;
			}
			
			std::cout << _edges_prior.size() << " == " << graph.getNumEdges() << std::endl;
			
			assert( _prior_landmark_positions.size() == graph.getNumVertices() );
// 			assert(_edges_prior.size() == graph.getNumEdges());
			
		}
		
		void makeGraph(){
			
			int id = 0;
			
			/******************Adding robot nodes*************/
			
			for (size_t i = 0; i < _robot_positions.size(); ++i) {
				std::cout << "Adding robot node " << i/3 << std::endl;
				g2o::VertexSE2* robot =  new g2o::VertexSE2;
				robot->setId(id);
				++id;
				Eigen::IOFormat fmt(Eigen::StreamPrecision, Eigen::DontAlignCols, "\t", " ", "", "", "", "");
// 				std::cout <<"Estimate " << isometry2d.matrix().format(cleanFmt) << std::endl;
// 				g2o::SE2 se2(_robot_positions[i](0), _robot_positions[i](1), _robot_positions[i](2));
				
				std::cout << "SE2 " << _robot_positions[i].toVector() << std::endl;
				robot->setEstimate(_robot_positions[i]);
				std::cout << "Robot " << robot->estimate().toVector() << std::endl;
				_optimizer.addVertex(robot);	
			}
			
			Eigen::Matrix3d covariance;
			covariance.fill(0.);
			covariance(0, 0) = _transNoise[0]*_transNoise[0];
			covariance(1, 1) = _transNoise[1]*_transNoise[1];
			covariance(2, 2) = _rotNoise*_rotNoise;
			Eigen::Matrix3d information = covariance.inverse();
			
			//Adding odometry
			for(size_t i = 0 ; i < _odometry.size() ; ++i){
				
				g2o::EdgeSE2* odometry = new g2o::EdgeSE2;
				odometry->vertices()[0] = _optimizer.vertex(std::get<1>(_odometry[i])) ;
				odometry->vertices()[1] = _optimizer.vertex(std::get<2>(_odometry[i])) ;
				odometry->setMeasurement(std::get<0>(_odometry[i]) /*simEdge.simulatorTransf*/);
				odometry->setInformation(information);
				_optimizer.addEdge(odometry);
				
				
			}
			
			/****************** Adding Landmarks nodes *************/
			
			Eigen::Matrix3d covariance_landmark; 
			covariance_landmark.fill(0.);
			covariance_landmark(0, 0) = _landmarkNoise[0]*_landmarkNoise[0];
			covariance_landmark(1, 1) = _landmarkNoise[1]*_landmarkNoise[1];
			covariance_landmark(2, 2) = 13;//<- Rotation covariance landmark is more than 4PI
			Eigen::Matrix3d information_landmark = covariance_landmark.inverse();
			
			std::cerr << "Optimization: add landmark vertices ... ";
			for (size_t i = 0; i < _landmark_positions.size() ; ++i) {
				std::cout << "Add landmark" << std::endl;
				g2o::VertexSE2* landmark = new g2o::VertexSE2;
				landmark->setId(id);
				++id;
// 				g2o::SE2 se2(_landmark_positions[i](0), _landmark_positions[i](1), _landmark_positions[i](2));
				landmark->setEstimate(_landmark_positions[i]);
				_optimizer.addVertex(landmark);
			}
			std::cerr << "done." << std::endl;

			std::cerr << "Optimization: add landmark observations ... ";
			for (size_t i = 0; i < _observation_real_landmarks.size(); ++i) {
				g2o::EdgeSE2* landmarkObservation =  new g2o::EdgeSE2;
				landmarkObservation->vertices()[0] = _optimizer.vertex(std::get<1>(_observation_real_landmarks[i]) );
				landmarkObservation->vertices()[1] = _optimizer.vertex(std::get<2>(_observation_real_landmarks[i]));
				landmarkObservation->setMeasurement(std::get<0>(_observation_real_landmarks[i]));
				landmarkObservation->setInformation(information_landmark);
				//TODO
				landmarkObservation->setParameterId(0, _sensorOffset->id());
				_optimizer.addEdge(landmarkObservation);
			}
			std::cerr << "done." << std::endl;
			
			/****************** Adding Prior nodes *************/
			
// 			Eigen::Matrix3d covariance_prior; 
// 			covariance_prior.fill(0.);
// 			covariance_prior(0, 0) = _priorNoise[0]*_priorNoise[0];
// 			covariance_prior(1, 1) = _priorNoise[1]*_priorNoise[1];
// 			covariance_prior(2, 2) = 13;//<- Rotation covariance prior landmark is more than 4PI
// 			Eigen::Matrix3d information_prior = covariance_prior.inverse();
			
			
			auto makeInformationMat = [this](int index, int index2) -> Eigen::Matrix3d{
				
				//Get Eigen vector
				Eigen::Vector3d pose1 = _prior_landmark_positions[index - _robot_positions.size() - _landmark_positions.size()].toVector();
				Eigen::Vector3d pose2 = _prior_landmark_positions[index2 - _robot_positions.size() - _landmark_positions.size()].toVector();
				
				Eigen::Vector2d eigenvec;
				eigenvec << pose1(0) - pose2(0), pose1(1) - pose2(1);
				std::pair<double, double> eigenval(_priorNoise(0), _priorNoise(1));
				
				Eigen::Matrix2d cov = getCovarianceVec(eigenvec, eigenval);
				Eigen::Matrix3d covariance_prior;
				covariance_prior.fill(0.);
				covariance_prior(0, 0) = cov(0, 0);
				covariance_prior(0, 1) = cov(0, 1);
				covariance_prior(1, 0) = cov(1, 0);
				covariance_prior(1, 1) = cov(1, 1);
				covariance_prior(2, 2) = 13;//<- Rotation covariance prior landmark is more than 4PI
				Eigen::Matrix3d information_prior = covariance_prior.inverse();
				
				return information_prior;
				
			};
			
			
			std::cerr << "Optimization: add prior landmark vertices ... ";
			for (size_t i = 0; i < _prior_landmark_positions.size(); ++i) {
				g2o::VertexSE2* landmark = new g2o::VertexSE2;
				landmark->setId(id);
				++id;
// 				g2o::SE2 se2(_prior_landmark_positions[i], _prior_landmark_positions[i+1], _prior_landmark_positions[i+2]);
// 				g2o::SE2 se2(_prior_landmark_positions[i](0), _prior_landmark_positions[i](1), _prior_landmark_positions[i](2));
				landmark->setEstimate(_prior_landmark_positions[i]);
				_optimizer.addVertex(landmark);
			}
			std::cerr << "done." << std::endl;
			
			std::cerr << "Optimization: add wall prior ... ";
			for (size_t i = 0; i < _edges_prior.size(); ++i) {
				g2o::EdgeSE2Prior_malcolm* landmarkObservation =  new g2o::EdgeSE2Prior_malcolm;
				landmarkObservation->vertices()[0] = _optimizer.vertex(std::get<1>(_edges_prior[i]));
				landmarkObservation->vertices()[1] = _optimizer.vertex(std::get<2>(_edges_prior[i]));
				landmarkObservation->setMeasurement(std::get<0>(_edges_prior[i]));
				Eigen::Matrix3d information_prior = makeInformationMat(std::get<1>(_edges_prior[i]), std::get<2>(_edges_prior[i]));
				landmarkObservation->setInformation(information_prior);
				//TODO
				landmarkObservation->setParameterId(0, _sensorOffset->id());
				_optimizer.addEdge(landmarkObservation);
			}
			
			/****************** Adding prior to ndt edges *************/
			
			Eigen::Matrix3d covariance_link; 
			covariance_link.fill(0.);
			covariance_link(0, 0) = _linkNoise[0]*_linkNoise[0];
			covariance_link(1, 1) = _linkNoise[1]*_linkNoise[1];
			covariance_link(2, 2) = 13;//<- Rotation covariance link is more than 4PI
			Eigen::Matrix3d information_link = covariance_link.inverse();
			
			//Add link between the two maps -> edge oftransform zero
			std::cerr << "Optimization: add wall prior link to ndt ... ";
			for (size_t i = 0; i < _link_in_between_maps.size(); ++i) {
				g2o::EdgeSE2Link_malcolm* landmarkObservation =  new g2o::EdgeSE2Link_malcolm;
				landmarkObservation->vertices()[0] = _optimizer.vertex(std::get<1>(_link_in_between_maps[i]));
				landmarkObservation->vertices()[1] = _optimizer.vertex(std::get<2>(_link_in_between_maps[i]));
				landmarkObservation->setMeasurement(std::get<0>(_link_in_between_maps[i]));
				landmarkObservation->setInformation(information_link);
				//TODO
				landmarkObservation->setParameterId(0, _sensorOffset->id());
				_optimizer.addEdge(landmarkObservation);
			}

			g2o::VertexSE2* firstRobotPose = dynamic_cast<g2o::VertexSE2*>(_optimizer.vertex(0));
			firstRobotPose->setFixed(true);
			
			std::cerr << "done." << std::endl;
			
			
		}
		
		
		
		
	};
}

#endif