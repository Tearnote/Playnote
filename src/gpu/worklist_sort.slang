/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

import lib;
#include "shared/worklist.slang.h"

RWStructuredBuffer<WorklistItem> b_worklists;
StructuredBuffer<int> b_worklistCounts;

groupshared var worklist: WorklistItem[WORKLIST_MAX_SIZE];

func bitonicSort<T, let N: int>(inout arr: T[N], threadId: int, comp: functype(T, T) -> bool)
{
	for (int k = 2; k <= N; k <<= 1) {
		for (int j = k >> 1; j > 0; j >>= 1) {
			let t = threadId;
			let i = (t & (j - 1)) | ((t & ~(j - 1)) << 1);
			let l = i + j;

			let a = arr[i];
			let b = arr[l];

			let up = (i & k) == 0;
			if (up == comp(a, b)) {
				arr[i] = b;
				arr[l] = a;
			}
			GroupMemoryBarrierWithGroupSync();
		}
	}
}

[shader("compute")]
[numthreads(WORKLIST_MAX_SIZE / 2)]
void computeMain(int2 threadId: SV_DispatchThreadID)
{
	let thread = threadId.x;
	let tile = threadId.y;
	let count = b_worklistCounts[tile];

	// Copy data to sort into group-shared memory
	let offset = tile * WORKLIST_MAX_SIZE;
	let readLoc1 = thread;
	let readLoc2 = thread + WORKLIST_MAX_SIZE / 2;
	worklist[readLoc1] = readLoc1 < count? b_worklists[offset + readLoc1] : WorklistItem(-1, -1);
	worklist[readLoc2] = readLoc2 < count? b_worklists[offset + readLoc2] : WorklistItem(-1, -1);
	GroupMemoryBarrierWithGroupSync();

	bitonicSort(worklist, thread, (WorklistItem a, WorklistItem b) => a.group_id < b.group_id);

	// Copy data back to global memory
	b_worklists[offset + thread] = worklist[thread];
	b_worklists[offset + thread + WORKLIST_MAX_SIZE / 2] = worklist[thread + WORKLIST_MAX_SIZE / 2];
}
