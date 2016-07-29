#include "PCL.hpp"

#include <pcl/filters/filter.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/obj_io.h>
#include <pcl/registration/icp.h>
#include <pcl/registration/sample_consensus_prerejective.h>


/************************************************************************/
/* Load model or mesh                                                  */
/************************************************************************/
/**
*  @brief LoadModel: load .pcd file to program, either model or mesh
*  @param model_path   .pcd filepath
*  @param model        load PointCloudT/PointCloudNT to memory
*/
bool  LoadModel(const string model_path, PointCloudT::Ptr &model) //XYZ
{
	pcl::ScopeTime t("[Load point clouds]");
	if (pcl::io::loadPCDFile<PointT>(model_path, *model) < 0) {
		pcl::console::print_error("Error loading model file!\n");
		return false;
	}
	return true;
}
bool  LoadModel(const string model_path, PointCloudNT::Ptr &model) //Normal
{
	pcl::ScopeTime t("[Load point clouds]");
	if (pcl::io::loadPCDFile<PointNT>(model_path, *model) < 0) {
		pcl::console::print_error("Error loading model file!\n");
		return false;
	}
	return true;
}

/************************************************************************/
/* Load grasping region point cloud                                     */
/************************************************************************/
bool loadGraspPcd(const string model_path, PointCloudT::Ptr &grasp)
{
	pcl::ScopeTime t("[Load grasping point regions]");
	if (pcl::io::loadPCDFile<PointT>(model_path, *grasp) < 0) {
		pcl::console::print_error("Error loading object file!\n");
		return false;
	}
	return true;
}
bool loadGraspPcd(const string model_path, PointCloudNT::Ptr &grasp)
{
	pcl::ScopeTime t("[Load grasping point regions]");
	if (pcl::io::loadPCDFile<PointNT>(model_path, *grasp) < 0) {
		pcl::console::print_error("Error loading object file!\n");
		return false;
	}
	return true;
}

bool loadGrasp(const string model_path, PointCloudT::Ptr &grasp)
{
	//TODO
	pcl::ScopeTime t("[Load grasping point regions]");
	if (pcl::io::loadOBJFile<PointT>(model_path, *grasp) < 0) {
		pcl::console::print_error("Error loading object file!\n");
		return false;
	}
	return true;
}

/************************************************************************/
/* Output Transformation Matrix                                         */
/************************************************************************/
void print4x4Matrix(const Matrix4f & matrix)
{
	print_info("Rotation matrix :\n");
	print_info("    | %6.3f %6.3f %6.3f | \n", matrix(0, 0), matrix(0, 1), matrix(0, 2));
	print_info("R = | %6.3f %6.3f %6.3f | \n", matrix(1, 0), matrix(1, 1), matrix(1, 2));
	print_info("    | %6.3f %6.3f %6.3f | \n", matrix(2, 0), matrix(2, 1), matrix(2, 2));
	print_info("Translation vector :\n");
	print_info("t = < %6.3f, %6.3f, %6.3f >\n\n", matrix(0, 3), matrix(1, 3), matrix(2, 3));
}

/************************************************************************/
/* Downsample model point cloud                                         */
/************************************************************************/
void Downsample(PointCloudNT::Ptr &model, float leaf)
{
	// Downsample
	pcl::VoxelGrid <PointNT> grid;
	grid.setLeafSize(leaf, leaf, leaf);
	grid.setInputCloud(model);
	grid.filter(*model);
}

/************************************************************************/
/* Estimate model curvatures                                            */
/************************************************************************/
void EstimateCurvatures(PointCloudNT::Ptr &model, float radius)
{
	pcl::PrincipalCurvaturesEstimation<PointNT, pcl::Normal, pcl::PrincipalCurvatures> principalCurvaturesEstimation;
	principalCurvaturesEstimation.setInputCloud(model);

}

/************************************************************************/
/* Estimate FPFH features                                               */
/************************************************************************/
void EstimateFPFH(PointCloudNT::Ptr &model, FeatureCloudT::Ptr &model_features, float leaf)
{
	FeatureEstimationT fest;
	fest.setRadiusSearch(5 * leaf);
	fest.setInputCloud(model);
	fest.setInputNormals(model);
	fest.compute(*model_features);
}

/************************************************************************/
/* Registration with RANSAC and ICP                                     */
/************************************************************************/
/**
*  @brief Registration: register model and mesh with RANSAC+ICP, out Transformation matrix
*  @param model        input 3D points cloud
*  @param mesh         generated by Depth camera and convert to Point Cloud Normal Point
*  @param model_align  output aligned model, which used to reflect to 2D
*  @param showGraphic  show graphic result or not
*/
Matrix4f Registration(	PointCloudNT::Ptr &model, 
						PointCloudNT::Ptr &mesh, 
						PointCloudNT::Ptr &model_align, 
						RegisterParameter &para,
						bool showGraphic) {
	// Point cloud
	FeatureCloudT::Ptr model_features(new FeatureCloudT);
	FeatureCloudT::Ptr mesh_features(new FeatureCloudT);
	Matrix4f transformation_ransac = Matrix4f::Identity();
	Matrix4f transformation_icp = Matrix4f::Identity();
	const float leaf = para.leaf; 

	pcl::visualization::PCLVisualizer viewer("RANSAC-ICP");
	{
		pcl::ScopeTime t("[Add init position]");

		viewer.addPointCloud(mesh, ColorHandlerNT(mesh, 255.0, 255.0, 255.0), "init_mesh");
		viewer.addPointCloud(model, ColorHandlerNT(model, 255.0, 255.0, 255.0), "init_model");
	}

	{
		pcl::ScopeTime t("[Downsample]");

		pcl::VoxelGrid <PointNT> grid;
		grid.setLeafSize(leaf, leaf, leaf);
		//grid.setInputCloud(model);
		//grid.filter(*model);
		grid.setInputCloud(mesh);
		grid.filter(*mesh);
	}
	{
		pcl::ScopeTime t("[Estimate normals for mesh]");
		NormalEstimationNT nest;
		nest.setRadiusSearch(leaf);
		nest.setInputCloud(mesh);
		nest.compute(*mesh);
		//nest.setInputCloud (model);
		//nest.compute(*model);
	}
	{
		pcl::ScopeTime t("[Estimate features]");
		FeatureEstimationT fest;
		fest.setRadiusSearch(5 * leaf);
		fest.setInputCloud(model);
		fest.setInputNormals(model);
		fest.compute(*model_features);
		fest.setInputCloud(mesh);
		fest.setInputNormals(mesh);
		fest.compute(*mesh_features);
	}
	// RANSAC
	pcl::SampleConsensusPrerejective <PointNT, PointNT, FeatureT> ransac;
	ransac.setInputSource(model);
	ransac.setSourceFeatures(model_features);
	ransac.setInputTarget(mesh);
	ransac.setTargetFeatures(mesh_features);
	ransac.setMaximumIterations(para.MaximumIterationsRANSAC); // Number of RANSAC iterations
	ransac.setNumberOfSamples(para.NumberOfSamples); // Number of points to sample for generating/prerejecting a pose
	ransac.setCorrespondenceRandomness(para.CorrespondenceRandomness); // Number of nearest features to use
	ransac.setSimilarityThreshold(para.SimilarityThreshold); // Polygonal edge length similarity threshold
	ransac.setMaxCorrespondenceDistance(para.MaxCorrespondence * leaf); // Inlier threshold
	ransac.setInlierFraction(para.InlierFraction); // Required inlier fraction for accepting a pose hypothesis
	{
		pcl::ScopeTime t("[RANSAC]");
		ransac.align(*model_align);
	}
	transformation_ransac = ransac.getFinalTransformation();
	if (!ransac.hasConverged()) {
		pcl::console::print_error("RANSAC alignment failed!\n");
		viewer.close();
		return transformation_ransac;
	}
	print4x4Matrix(transformation_ransac);
	//If RANSAC success, then ICP
	//ICP
	pcl::IterativeClosestPoint<PointNT, PointNT> icp; //ICP algorithm
	//icp.setInputSource(model_align);
	//icp.setInputTarget(mesh);
	icp.setInputSource(mesh);
	icp.setInputTarget(model_align);
	icp.setEuclideanFitnessEpsilon(para.EuclideanEpsilon);
	icp.setMaximumIterations(para.MaximumIterationsICP);
	icp.setTransformationEpsilon(para.EuclideanEpsilon);
	{
		pcl::ScopeTime t("[ICP]");
		icp.align(*mesh);
	}
	transformation_icp = icp.getFinalTransformation();
	print4x4Matrix(transformation_icp);
	//trans model with inverse matrix
	//Eigen::Matrix4f inverse = transformation_icp.inverse() * transformation;
	Eigen::Matrix4f inverse = transformation_icp.inverse();
	pcl::transformPointCloud(*model_align, *model_align, inverse);

	//pcl::transformPointCloud(*model, *model, inverse);
	if (showGraphic) {
		//Show graphic result
		viewer.addPointCloud(mesh, ColorHandlerNT(mesh, 0.0, 255.0, 0.0), "mesh");
		viewer.addPointCloud(model, ColorHandlerNT(model, 0.0, 0.0, 255.0), "model");
		viewer.addPointCloud(model_align, ColorHandlerNT(model_align, 0.0, .0, 255.0), "model_align");
		
		//viewer.addPointCloud(grasp, ColorHandlerNT(grasp, 0.0, 255.0, 255.0), "grasp");
		
		viewer.spin();
	}
	viewer.close();
	return inverse * transformation_ransac;
}

Matrix4f RegistrationNoShow(PointCloudNT::Ptr &model, PointCloudNT::Ptr &mesh, PointCloudNT::Ptr &model_align, RegisterParameter &para) {
	// Point cloud
	FeatureCloudT::Ptr model_features(new FeatureCloudT);
	FeatureCloudT::Ptr mesh_features(new FeatureCloudT);
	Matrix4f transformation_ransac = Matrix4f::Identity();
	Matrix4f transformation_icp = Matrix4f::Identity();
	const float leaf = para.leaf; 
	{
		pcl::ScopeTime t("[Downsample]");

		pcl::VoxelGrid <PointNT> grid;
		grid.setLeafSize(leaf, leaf, leaf);
		//grid.setInputCloud(model);
		//grid.filter(*model);
		grid.setInputCloud(mesh);
		grid.filter(*mesh);
	}
	{
		pcl::ScopeTime t("[Estimate normals for mesh]");
		NormalEstimationNT nest;
		nest.setRadiusSearch(leaf);
		nest.setInputCloud(mesh);
		nest.compute(*mesh);
	}
	{
		pcl::ScopeTime t("[Estimate features]");
		FeatureEstimationT fest;
		fest.setRadiusSearch(5 * leaf);
		fest.setInputCloud(model);
		fest.setInputNormals(model);
		fest.compute(*model_features);
		fest.setInputCloud(mesh);
		fest.setInputNormals(mesh);
		fest.compute(*mesh_features);
	}
	// RANSAC
	pcl::SampleConsensusPrerejective <PointNT, PointNT, FeatureT> ransac;
	ransac.setInputSource(model);
	ransac.setSourceFeatures(model_features);
	ransac.setInputTarget(mesh);
	ransac.setTargetFeatures(mesh_features);
	ransac.setMaximumIterations(para.MaximumIterationsRANSAC); // Number of RANSAC iterations
	ransac.setNumberOfSamples(para.NumberOfSamples); // Number of points to sample for generating/prerejecting a pose
	ransac.setCorrespondenceRandomness(para.CorrespondenceRandomness); // Number of nearest features to use
	ransac.setSimilarityThreshold(para.SimilarityThreshold); // Polygonal edge length similarity threshold
	ransac.setMaxCorrespondenceDistance(para.MaxCorrespondence * leaf); // Inlier threshold
	ransac.setInlierFraction(para.InlierFraction); // Required inlier fraction for accepting a pose hypothesis
	{
		pcl::ScopeTime t("[RANSAC]");
		ransac.align(*model_align);
	}
	transformation_ransac = ransac.getFinalTransformation();
	if (!ransac.hasConverged()) {
		pcl::console::print_error("RANSAC alignment failed!\n");
		// return transformation_ransac;
	}
	print4x4Matrix(transformation_ransac);
	//If RANSAC success, then ICP
	//ICP
	int iterations = 100;
	double EuclideanEpsilon = 2e-8;
	int MaximumIterations = 1000;
	pcl::IterativeClosestPoint<PointNT, PointNT> icp; //ICP algorithm
	//icp.setInputSource(model_align);
	//icp.setInputTarget(mesh);
	icp.setInputSource(mesh);
	icp.setInputTarget(model_align);
	icp.setEuclideanFitnessEpsilon(EuclideanEpsilon);
	icp.setMaximumIterations(MaximumIterations);
	icp.setTransformationEpsilon(EuclideanEpsilon);
	{
		pcl::ScopeTime t("[ICP]");
		icp.align(*mesh);
	}
	transformation_icp = icp.getFinalTransformation();
	print4x4Matrix(transformation_icp);
	Eigen::Matrix4f inverse = transformation_icp.inverse();
	pcl::transformPointCloud(*model_align, *model_align, inverse);
	
	return inverse * transformation_ransac;
}

Matrix4f RegistrationNoShow_ICP(	PointCloudNT::Ptr &model, 
									PointCloudNT::Ptr &mesh, 
									PointCloudNT::Ptr &model_align, 
									RegisterParameter &para) 
{
	// Point cloud
	FeatureCloudT::Ptr model_features(new FeatureCloudT);
	FeatureCloudT::Ptr mesh_features(new FeatureCloudT);
	Matrix4f transformation_ransac = Matrix4f::Identity();
	Matrix4f transformation_icp = Matrix4f::Identity();
	const float leaf = para.leaf;
	{
		pcl::ScopeTime t("[Downsample]");

		pcl::VoxelGrid <PointNT> grid;
		grid.setLeafSize(leaf, leaf, leaf);
		//grid.setInputCloud(model);
		//grid.filter(*model);
		grid.setInputCloud(mesh);
		grid.filter(*mesh);
	}
	{
		pcl::ScopeTime t("[Estimate normals for mesh]");
		NormalEstimationNT nest;
		nest.setRadiusSearch(leaf);
		nest.setInputCloud(mesh);
		nest.compute(*mesh);
	}
	{
		pcl::ScopeTime t("[Estimate features]");
		FeatureEstimationT fest;
		fest.setRadiusSearch(5 * leaf);
		fest.setInputCloud(model);
		fest.setInputNormals(model);
		fest.compute(*model_features);
		fest.setInputCloud(mesh);
		fest.setInputNormals(mesh);
		fest.compute(*mesh_features);
	}

	//If RANSAC success, then ICP
	//ICP
	int iterations = 100;
	double EuclideanEpsilon = 2e-8;
	int MaximumIterations = 1000;
	pcl::IterativeClosestPoint<PointNT, PointNT> icp; //ICP algorithm
	//icp.setInputSource(model_align);
	//icp.setInputTarget(mesh);
	icp.setInputSource(mesh);
	icp.setInputTarget(model);
	icp.setEuclideanFitnessEpsilon(EuclideanEpsilon);
	icp.setMaximumIterations(MaximumIterations);
	icp.setTransformationEpsilon(EuclideanEpsilon);
	{
		pcl::ScopeTime t("[ICP]");
		icp.align(*mesh);
	}
	transformation_icp = icp.getFinalTransformation();
	print4x4Matrix(transformation_icp);
	Eigen::Matrix4f inverse = transformation_icp.inverse();
	pcl::transformPointCloud(*model_align, *model_align, inverse);

	return inverse * transformation_ransac;
}