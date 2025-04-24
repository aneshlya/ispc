// Flash Attention 2 implementation in ISPC
#define MAX_TILE_SIZE 1024
#define MAX_HEAD_DIM 1024
#define TILE_SIZE 16

export void single_head_attention(
    uniform float Q[], uniform float K[], uniform float V[],
    uniform float output[], 
    uniform int seq_len, uniform int d_k) {
    // Calculate number of tiles needed
    uniform int num_tiles = (seq_len + TILE_SIZE - 1) / TILE_SIZE;
    uniform float sm_scale = rsqrt((uniform float)d_k); // Softmax scaling factor
    
    // Process each query tile
    for (uniform int tile_q = 0; tile_q < num_tiles; tile_q++) {
        uniform int q_start = tile_q * TILE_SIZE;
        uniform int q_end = min(q_start + TILE_SIZE, seq_len);
        
        // Process each query position within the tile
        for (uniform int q_offset = 0; q_offset < q_end - q_start; q_offset++) {
            uniform int q_idx = q_start + q_offset;
            
            // Skip if beyond sequence length
            if (q_idx >= seq_len) continue;
            
            // Initialize tracking variables for numerically stable softmax
            uniform float m_i = -1e30f;        // Running maximum value
            uniform float l_i = 0.0f;          // Running softmax denominator
            
            // Initialize output accumulator
            uniform float acc[MAX_HEAD_DIM];
            for (uniform int dim = 0; dim < d_k; dim++) {
                acc[dim] = 0.0f;
            }
            
            // Process each key/value tile
            for (uniform int tile_k = 0; tile_k < num_tiles; tile_k++) {
                uniform int k_start = tile_k * TILE_SIZE;
                uniform int k_end = min(k_start + TILE_SIZE, seq_len);
                uniform int k_tile_len = k_end - k_start;
                
                // Arrays to store current tile's attention scores and vectors
                uniform float qk[MAX_TILE_SIZE];        // QK^T scores before exp
                uniform float p[MAX_TILE_SIZE];         // Attention weights after exp
                
                // Step 1: Compute attention scores for each key in the tile
                uniform float m_ij = -1e30f;           // Maximum score for this tile
                
                for (uniform int k_offset = 0; k_offset < k_tile_len; k_offset++) {
                    uniform int k_idx = k_start + k_offset;
                    
                    // Compute dot product between query and key vectors
                    uniform float dot = 0.0f;
                    for (uniform int dim = 0; dim < d_k; dim++) {
                        dot += Q[q_idx * d_k + dim] * K[k_idx * d_k + dim];
                    }
                    
                    // Apply softmax scaling
                    qk[k_offset] = dot * sm_scale;
                    
                    // Track maximum score
                    m_ij = max(m_ij, qk[k_offset]);
                }
                
                // Step 2: Update the global maximum and compute adjustment factors
                uniform float m_new = max(m_i, m_ij);
                uniform float alpha = exp(m_i - m_new);     // Scale factor for previous values
                
                // Step 3: Compute exponentials and sum for current tile
                uniform float l_ij = 0.0f;                  // Local softmax denominator for this tile
                
                for (uniform int k_offset = 0; k_offset < k_tile_len; k_offset++) {
                    // Compute exp(QK^T - m_new) for numerical stability
                    p[k_offset] = exp(qk[k_offset] - m_new);
                    l_ij += p[k_offset];                    // Accumulate softmax denominator
                }
                
                // Step 4: Update global softmax denominator
                l_i = alpha * l_i + l_ij;
                
                // Step 5: Update accumulated output with the weighted values
                for (uniform int dim = 0; dim < d_k; dim++) {
                    // Scale existing accumulation by alpha to account for max change
                    acc[dim] *= alpha;
                    
                    // Add contributions from current tile
                    uniform float value_sum = 0.0f;
                    for (uniform int k_offset = 0; k_offset < k_tile_len; k_offset++) {
                        uniform int k_idx = k_start + k_offset;
                        value_sum += p[k_offset] * V[k_idx * d_k + dim];
                    }
                    acc[dim] += value_sum;
                }
                
                // Update running maximum
                m_i = m_new;
            }
            
            // Normalize accumulated values by final softmax denominator
            for (uniform int dim = 0; dim < d_k; dim++) {
                if (l_i > 0.0f) {
                    output[q_idx * d_k + dim] = acc[dim] / l_i;
                } else {
                    output[q_idx * d_k + dim] = 0.0f;
                }
            }
        }
    }
}