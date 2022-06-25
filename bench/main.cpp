#include <benchmark/benchmark.h>

#include <nonrec.h>

#include <cassert>

auto tall_dfs_graph() {
    int n = 100'000;
    std::vector<std::vector<int>> graph(n);
    for (int i = 0; i < n - 1; i++) {
        graph[i].push_back(i + 1);
        graph[i + 1].push_back(i);
    }
    return graph;
}

auto wide_dfs_graph() {
    int n = 100'000;
    std::vector<std::vector<int>> graph(n);
    auto add_edge = [&](int u, int v) {
        graph[u].push_back(v);
        graph[v].push_back(u);
    };
    for (int i = 0; i < n; i++) {
        if (2 * i + 1 < n) {
            add_edge(i, 2 * i + 1);
        }
        if (2 * i + 2 < n) {
            add_edge(i, 2 * i + 2);
        }
    }
    return graph;
}

struct rec_dfs {
    const std::vector<std::vector<int>>& graph;
    std::vector<bool> vis;

    int operator()(int v) {
        vis[v] = true;
        int ret = 1;
        for (int u : graph[v]) {
            if (!vis[u]) {
                ret += (*this)(u);
            }
        }
        return ret;
    }

    void benchmark(benchmark::State& state) {
        for (auto _ : state) {
            vis.assign(graph.size(), false);
            int N = (*this)(0);
            assert(N == graph.size());
        }
    }
};

struct nonrecursive_dfs {
    const std::vector<std::vector<int>>& graph;

    int operator()(int v) {
        std::vector<bool> vis(graph.size(), false);
        std::stack<std::pair<int, int>> stack;
        std::stack<int> ret;
        ret.push(0);
        stack.push({0, 0});
        ret.push(1);
        while (!stack.empty()) {
            auto [v, i] = stack.top();
            vis[v] = true;
            stack.pop();
            if (i == static_cast<int>(graph[v].size())) {
                int ret_val = ret.top();
                ret.pop();
                ret.top() += ret_val;
                continue;
            }
            stack.emplace(v, i + 1);
            int u = graph[v][i];
            if (!vis[u]) {
                stack.emplace(u, 0);
                ret.push(1);
            }
        }
        return ret.top();
    }

    void benchmark(benchmark::State& state) {
        for (auto _ : state) {
            int N = (*this)(0);
            assert(N == graph.size());
        }
    }
};

struct nonrec_dfs {
    const std::vector<std::vector<int>>& graph;
    std::vector<bool> vis;

    nr::nonrec<int> operator()(int v) {
        vis[v] = true;
        int ret = 1;
        for (int u : graph[v]) {
            if (!vis[u]) {
                ret += co_await (*this)(u);
            }
        }
        co_return ret;
    }

    void benchmark(benchmark::State& state) {
        for (auto _ : state) {
            vis.assign(graph.size(), false);
            int N = (*this)(0).get();
            assert(N == graph.size());
        }
    }
};

void tall_dfs_rec(benchmark::State &state) {
    rec_dfs{tall_dfs_graph()}.benchmark(state);
}
BENCHMARK(tall_dfs_rec);

void tall_dfs_nonrecursive(benchmark::State &state) {
    nonrecursive_dfs{tall_dfs_graph()}.benchmark(state);
}
BENCHMARK(tall_dfs_nonrecursive);

void tall_dfs_nonrec(benchmark::State &state) {
    nonrec_dfs{tall_dfs_graph()}.benchmark(state);
}
BENCHMARK(tall_dfs_nonrec);

void wide_dfs_rec(benchmark::State &state) {
    rec_dfs{wide_dfs_graph()}.benchmark(state);
}
BENCHMARK(wide_dfs_rec);

void wide_dfs_nonrecursive(benchmark::State &state) {
    nonrecursive_dfs{wide_dfs_graph()}.benchmark(state);
}
BENCHMARK(wide_dfs_nonrecursive);

void wide_dfs_nonrec(benchmark::State &state) {
    nonrec_dfs{wide_dfs_graph()}.benchmark(state);
}
BENCHMARK(wide_dfs_nonrec);

BENCHMARK_MAIN();
