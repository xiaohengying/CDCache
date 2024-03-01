#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "io_request.h"
#include "storage_system.h"
#include "utils.h"

const std::string version_info = "Cache app\n";
void printHelpInfo() {
    printf("BUILD Time: %s %s\n", __DATE__, __TIME__);
    printf("%s", version_info.c_str());
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printHelpInfo();
        printf("Use ./main_cache_app [config path]\n");
        return -1;
    }

    Assert(globalEnv().init(argv[1]), "Can not read config file!");
    globalEnv().c.dump(stdout);
    AsyncIORequestReader reader(globalEnv().c.dataset_trace_path,   //
                                globalEnv().c.dataset_data_path,    //
                                globalEnv().c.dataset_block_size);  //

    StorageSystem s{};
    s.setUseCache(globalEnv().c.use_cache);
    s.init(globalEnv().c.primary_name, globalEnv().c.primary_size,  // primary
           globalEnv().c.cache_name, globalEnv().c.cache_size);     // cache
    auto total_request = reader.size();
    globalEnv().startEvaluation();
    auto start_time = get_timestamp();
    auto last_step_time = start_time;
    while (reader.hasNext()) {
        auto req = reader.getRequest();
        auto writeDataBlock = IORequest::toDataBlock(req);
        if (req.type == Write) {
            globalEnv().write("[%d] WRITE %lx", globalEnv().index(), writeDataBlock.raw_fp());
            s.processIORequest(req);
        } else if (req.type == Read) {
            auto data_block = IORequest::toDataBlock(req);
            s.processIORequest(req);
            auto newC = IORequest::toDataBlock(req);
            globalEnv().write("[%d] READ %lx %d", globalEnv().index(), data_block.raw_fp(), !newC.raw_data().empty());
        }
        globalEnv().update_index();
        // print the progress
        if (globalEnv().index() % (total_request / 10) == 0) {
            auto current_time = get_timestamp();
            auto finished = globalEnv().index() * 100.0 / static_cast<double>(total_request);
            auto time_total_used = static_cast<double>(current_time - start_time);
            auto time_last_step_used = static_cast<double>(current_time - last_step_time);
            last_step_time = current_time;  // update last time

            auto time_left = time_total_used * 1.0 * static_cast<double>(total_request - globalEnv().index()) /
                             (globalEnv().index() * 60.0);
            printf("%.3lf%%  finished (+%.2lf Mins), %.2lf Minutes left\n", finished, time_last_step_used / 60.0,
                   time_left);
        }
    }
    globalEnv().finishEvaluation();
    printf("Finished\n");
    return 0;
}
