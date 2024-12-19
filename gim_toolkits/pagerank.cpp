/*
Copyright (c) 2014-2015 Xiaowei Zhu, Tsinghua University

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>

#include "core/gim_graph.hpp"
#include "mpi.h"

#include <math.h>

const double d = (double)0.85;
double exec_time = 0;
void compute(Graph<Empty>* graph, int iterations) {

    exec_time -= get_time();
    double** global_curr = graph->alloc_global_vertex_array<double>();
    double** global_next = graph->alloc_global_vertex_array<double>();
    VertexSubset** global_active = graph->alloc_global_vertex_subset();
    MPI_Barrier(MPI_COMM_WORLD);
    double* curr = global_curr[graph->partition_id];
    double* next = global_next[graph->partition_id];
    VertexSubset* active = global_active[graph->partition_id];
    // double* curr = graph->alloc_vertex_array<double>();
    // double* next = graph->alloc_vertex_array<double>();
    // VertexSubset* active = graph->alloc_vertex_subset();
    
    active->fill();   //处理所有点
    
    //当前PR值除以出度
    // double delta = graph->process_vertices<double>(
    //     [&](VertexId vtx) {
    //         curr[vtx] = (double)1;
    //         if (graph->out_degree[vtx] > 0) {
    //             curr[vtx] /= graph->out_degree[vtx];
    //         }
    //         return (double)1;
    //     },
    //     active);
    double delta = graph->process_vertices_global<double>(
        [&](VertexId vtx,int partition_id) {
            if(partition_id==-1){
                curr[vtx] = (double)1;
                if (graph->out_degree[vtx] > 0) {
                    curr[vtx] /= graph->out_degree[vtx];
                }
                return (double)1;
            }else{
                global_curr[partition_id][vtx] = (double)1;
                if (graph->gim_out_degree[partition_id][vtx] > 0) {
                    global_curr[partition_id][vtx] /= graph->gim_out_degree[partition_id][vtx];
                }
                return (double)1;
            }
        },
        global_active);
    delta /= graph->vertices;
    for (int i_i = 0; i_i < iterations; i_i++) {
        if (graph->partition_id==0) {
          printf("delta(%d)=%lf\n", i_i, delta);
        }
        graph->fill_vertex_array(next, (double)0);
        //稀疏模式下sendbuffer放pr值，稠密模式下sendbuffer放sum
        graph->process_edges<int, double>(
            [&](VertexId src) { graph->emit(src, curr[src]); },
            [&](VertexId src, double msg, VertexAdjList<Empty> outgoing_adj, int partition_id) {
                if (partition_id==-1) {
                    for (AdjUnit<Empty>* ptr = outgoing_adj.begin; ptr != outgoing_adj.end; ptr++) {
                        VertexId dst = ptr->neighbour;
                        write_add(&next[dst], msg);
                    }
                    return 0;
                }else{
                    for (AdjUnit<Empty>* ptr = outgoing_adj.begin; ptr != outgoing_adj.end; ptr++) {
                        VertexId dst = ptr->neighbour;
                        write_add(&global_next[partition_id][dst], msg);
                    }
                    return 0;
                }
            },
            [&](VertexId dst, VertexAdjList<Empty> incoming_adj, int partition_id) {
                double sum = 0;
                for (AdjUnit<Empty>* ptr = incoming_adj.begin; ptr != incoming_adj.end; ptr++) {
                    VertexId src = ptr->neighbour;
                    sum += curr[src];
                }
                graph->emit(dst, sum);
            },
            [&](VertexId dst, double msg) {
                write_add(&next[dst], msg);
                return 0;
            },
            active);
        if (i_i == iterations - 1) {
            // delta = graph->process_vertices<double>(
            //     [&](VertexId vtx) {
            //         next[vtx] = 1 - d + d * next[vtx];
            //         return 0;
            //     },
            //     active);
            delta = graph->process_vertices_global<double>(
                [&](VertexId vtx, int partition_id) {
                    if(partition_id==-1){
                        next[vtx] = 1 - d + d * next[vtx];
                    }else{
                        global_next[partition_id][vtx] = 1 - d + d * global_next[partition_id][vtx];
                    }
                    
                    return 0;
                },
                global_active);
        } else {
            // delta = graph->process_vertices<double>(
            //     [&](VertexId vtx) {
            //         next[vtx] = 1 - d + d * next[vtx];
            //         if (graph->out_degree[vtx] > 0) {
            //             next[vtx] /= graph->out_degree[vtx];
            //             return fabs(next[vtx] - curr[vtx]) * graph->out_degree[vtx];
            //         }
            //         return fabs(next[vtx] - curr[vtx]);
            //     },
            //     active);
            delta = graph->process_vertices_global<double>(
                [&](VertexId vtx, int partition_id) {
                    if(partition_id==-1){
                        next[vtx] = 1 - d + d * next[vtx];
                        if (graph->out_degree[vtx] > 0) {
                            next[vtx] /= graph->out_degree[vtx];
                            return fabs(next[vtx] - curr[vtx]) * graph->out_degree[vtx];
                        }
                        return fabs(next[vtx] - curr[vtx]);
                    }else{
                        global_next[partition_id][vtx] = 1 - d + d * global_next[partition_id][vtx];
                        if (graph->gim_out_degree[partition_id][vtx] > 0) {
                            global_next[partition_id][vtx] /=
                                graph->gim_out_degree[partition_id][vtx];
                            return fabs(global_next[partition_id][vtx] -
                                        global_curr[partition_id][vtx]) *
                                   graph->gim_out_degree[partition_id][vtx];
                        }
                        return fabs(global_next[partition_id][vtx] -
                                    global_curr[partition_id][vtx]);
                    }
                    
                },
                global_active);
        }
        delta /= graph->vertices;
        std::swap(curr, next);
        std::swap(global_curr,global_next);
    }

    exec_time += get_time();
    // if (graph->partition_id==0) {
    //   printf("exec_time=%lf(s)\n", exec_time);
    // }
    printf("partition: %d,exec_time=%lf(s)\n", graph->get_partition_id(), exec_time);
    // double pr_sum =
    //     graph->process_vertices<double>([&](VertexId vtx) { return curr[vtx]; }, active);
    double pr_sum = graph->process_vertices_global<double>(
        [&](VertexId vtx, int partition_id) { if(partition_id==-1){
            return curr[vtx];
        }else{
            return global_curr[partition_id][vtx];
        } }, global_active);
    if (graph->partition_id == 0) {
        printf("pr_sum=%lf\n", pr_sum);
    }

    graph->gather_vertex_array(curr, 0);
    if (graph->partition_id == 0) {
        VertexId max_v_i = 0;
        for (VertexId v_i = 0; v_i < graph->vertices; v_i++) {
            if (curr[v_i] > curr[max_v_i]) max_v_i = v_i;
        }
        printf("pr[%u]=%lf\n", max_v_i, curr[max_v_i]);
    }

    graph->dealloc_vertex_array(curr);
    graph->dealloc_vertex_array(next);
    delete active;
}

int main(int argc, char** argv) {
    MPI_Instance mpi(&argc, &argv);

    if (argc < 4) {
        printf("pagerank [file] [vertices] [iterations]\n");
        exit(-1);
    }
    if (numa_available() < 0) {
        printf("Your system does not support NUMA API\n");
    }

    Graph<Empty>* graph;
    graph = new Graph<Empty>();
    graph->load_directed(argv[1], std::atoi(argv[2]));
    printf("load complete\n");
    int iterations = std::atoi(argv[3]);

    compute(graph, iterations);
    printf("partiton_id: %d, total_process_time  =%lf(s)\n",
           graph->get_partition_id(),
           graph->print_total_process_time());
    // for (int run=0;run<5;run++) {
    //   compute(graph, iterations);
    // }
    delete graph;
    return 0;
}
