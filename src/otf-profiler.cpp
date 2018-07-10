/*
 This is part of the OTF-Profiler. Copyright by ZIH, TU Dresden 2016-2018.
 Authors: Maximillian Neumann, Denis Hünich, Jens Doleschal
*/

#include "otf-profiler.h"
#include "tracereader.h"
#include "utils.h"

#ifdef HAVE_CUBE
#include "create_cube.h"
#endif /* HAVE_CUBE*/

#ifdef OTFPROFILER_MPI
#include <mpi.h>
#include "reduce_data.h"
#endif /* OTFPROFILER_MPI */

using namespace std;

int error() {
#ifdef OTFPROFILER_MPI
    MPI_Abort(MPI_COMM_WORLD, 1);
#endif /* OTFPROFILER_MPI */

    return 1;
}

int main(int argc, char** argv) {
#ifdef OTFPROFILER_MPI
    /* start MPI */

    int my_rank;
    int num_ranks;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks);

    AllData alldata(my_rank, num_ranks);
#else  /* OTFPROFILER_MPI */
    AllData alldata(0, 1);
#endif /* OTFPROFILER_MPI */

    /* step 0: parse command line options */
    if (!alldata.params.parseCommandLine(argc, argv))
        return error();

    /* registers all scopes for time measurement depending on the verbose level */
    if (alldata.params.verbose_level > 0)
        alldata.tm.registerScope(ScopeID::TOTAL, "Total time");

    if (alldata.params.verbose_level > 1) {
        alldata.tm.registerScope(ScopeID::COLLECT, "collection data process");
        alldata.tm.registerScope(ScopeID::REDUCE, "reduce data");
        alldata.tm.registerScope(ScopeID::CUBE, "Cube creation process");
    }

    /* starts runtime measurement for total time */
    alldata.tm.start(ScopeID::TOTAL);

    /* start runtime measurement for collecting data */
    alldata.tm.start(ScopeID::COLLECT);
/* step 1: collect data by reading input trace file */
#ifdef OTFPROFILER_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif /* OTFPROFILER_MPI */
    unique_ptr<TraceReader> reader = getTraceReader(alldata);

    if (reader == nullptr)
        return error();

    if (!reader->initialize(alldata) || !reader->readDefinitions(alldata) ||
        !reader->readEvents(alldata))
        return error();

    reader.reset(nullptr);
#ifdef OTFPROFILER_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif /* OTFPROFILER_MPI */
    alldata.tm.stop(ScopeID::COLLECT);

#ifdef OTFPROFILER_MPI
    if (1 < alldata.metaData.numRanks) {
        /* step 4: reduce data to master; summarized data for producing
           LaTeX output; per-process/function statistics for additional
           clustering */

        MPI_Barrier(MPI_COMM_WORLD);
        alldata.tm.start(ScopeID::REDUCE);
        if (!ReduceData(alldata))
            return error();
        MPI_Barrier(MPI_COMM_WORLD);
        alldata.tm.stop(ScopeID::REDUCE);
    }
#endif /* OTFPROFILER_MPI */

#ifdef HAVE_CUBE
    if (alldata.params.create_cube) {
        /* step 6.3: create CUBE output */
        alldata.tm.start(ScopeID::CUBE);
        CreateCube(alldata);
        alldata.tm.stop(ScopeID::CUBE);
    }
#endif

    alldata.tm.stop(ScopeID::TOTAL);
#ifdef SHOW_RESULTS
    /* step 6.3: show result data on stdout */

    if (0 == alldata.myRank)
        show_results(alldata);
#endif /* SHOW_RESULTS */

    if (0 == alldata.metaData.myRank) {
        /* print runtime measurement results to stdout */
        alldata.tm.printAll();
    }

    alldata.verbosePrint(1, true, "done");

#ifdef OTFPROFILER_MPI
    MPI_Finalize();
#endif /* OTFPROFILER_MPI */

    return 0;
}