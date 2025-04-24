#define MAX_TILE_SIZE 1024
#define MAX_HEAD_DIM 1024
#define TILE_SIZE 16

export void single_head_attention(uniform float Q[], uniform float K[], uniform float V[], uniform float output[],
                                  uniform int seq_len, uniform int d_k) {
    // Calculate number of tiles needed
    uniform int num_tiles = (seq_len + TILE_SIZE - 1) / TILE_SIZE;
    uniform float sm_scale = rsqrt((uniform float)d_k); // Softmax scaling factor

    // Process each query tile
    for (uniform int tile_q = 0; tile_q < num_tiles; tile_q++) {
        uniform int q_start = tile_q * TILE_SIZE;
        uniform int q_end = min(q_start + TILE_SIZE, seq_len);
        uniform int q_tile_len = q_end - q_start;

        // Process each query position within the tile
        for (uniform int q_idx = q_start; q_idx < q_end; q_idx++) {

            // Initialize tracking variables for numerically stable softmax
            uniform float max_score = -1e30f; // Running maximum value
            uniform float sum_exp = 0.0f;   // Running softmax denominator

            // Initialize output accumulator
            uniform float acc[MAX_HEAD_DIM];

            foreach (i = 0 ... d_k) {
                acc[i] = 0.0f;
            }

            // Process each key/value tile
            for (uniform int tile_k = 0; tile_k < num_tiles; tile_k++) {
                uniform int k_start = tile_k * TILE_SIZE;
                uniform int k_end = min(k_start + TILE_SIZE, seq_len);
                uniform int k_tile_len = k_end - k_start;

                // Arrays to store current tile's attention scores and vectors
                uniform float scores[MAX_TILE_SIZE]; // QK^T scores before exp
                uniform float exp_scores[MAX_TILE_SIZE];  // Attention weights after exp

                // Step 1: Compute attention scores for each key in the tile
                uniform float tile_max_score = -1e30f; // Maximum score for this tile

                // Compute dot products (Q[q_idx], K[k_start:k_end])
                for (uniform int k_idx = k_start; k_idx< k_end; k_idx++) {
                    varying float dot_product = 0.0f;

                    foreach (dim = 0 ... d_k) {
                        dot_product += Q[q_idx * d_k + dim] * K[k_idx * d_k + dim];
                    }

                    // Reduce the dot product to a uniform value
                    uniform float dot = reduce_add(dot_product) * sm_scale;

                    // Apply softmax scaling
                    scores[k_idx - k_start] = dot;

                    // Track maximum score
                    tile_max_score = max(tile_max_score, dot);
                }


                // Step 2: Update the global maximum and compute adjustment factors
                uniform float new_max_score = max(max_score, tile_max_score);
                uniform float alpha = exp(max_score - new_max_score); // Scale factor for previous values

                // Step 3: Compute exponentials and sum for current tile
                float tile_sum_exp = 0.0f; // Local softmax denominator for this tile

                foreach (k_offset = 0 ... k_tile_len) {
                    // Compute exp(QK^T - max_score) for numerical stability
                    exp_scores[k_offset] = exp(scores[k_offset] - new_max_score);
                    tile_sum_exp += exp_scores[k_offset]; // Accumulate softmax denominator
                }

                // Step 4: Update global softmax denominator
                sum_exp = alpha * sum_exp + reduce_add(tile_sum_exp);

                // Process dimensions in chunks
                foreach (dim = 0 ... d_k) {
                    // Scale existing accumulation by alpha
                    acc[dim] *= alpha;

                    // Add contributions from current tile
                    varying float value_sum = 0.0f;

                    // Keep k_offset as uniform since exp_scores[] is a uniform array
                    for (uniform int k_offset = 0; k_offset < k_tile_len; k_offset++) {
                        uniform int k_idx = k_start + k_offset;
                        value_sum += exp_scores[k_offset] * V[k_idx * d_k + dim];
                    }

                    acc[dim] += value_sum;
                }

                // Update running maximum
                max_score = new_max_score;
            }

            foreach (dim = 0 ... d_k) {
                output[q_idx * d_k + dim] = (sum_exp > 0.0f) ? acc[dim] / sum_exp : 0.0f;
            }
        }
    }
}