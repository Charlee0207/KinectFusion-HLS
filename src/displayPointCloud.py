import open3d as o3d
import numpy as np
import argparse

parser = argparse.ArgumentParser(description="Load and visualize a PLY point cloud.")
parser.add_argument("filepath", type=str, help="Path to the PLY file.")

args = parser.parse_args()

print("Load a ply point cloud, print it, and render it")
pcd = o3d.io.read_point_cloud(args.filepath)
print(pcd)
print(np.asarray(pcd.points))
print("Origin point cloud")
o3d.visualization.draw_geometries([pcd],
                                  zoom=0.3412,
                                  front=[0, 0, -1],
                                  lookat=[128, 128, 128],
                                  up=[0, 1, 0])

print("Downsample the point cloud with a voxel of 0.01")
downpcd = pcd.voxel_down_sample(voxel_size=0.01)
o3d.visualization.draw_geometries([downpcd],
                                  zoom=0.3412,
                                  front=[0, 0, -1],
                                  lookat=[128, 128, 128],
                                  up=[0, 1, 0])
