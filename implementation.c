#include "implementation.h"

void read_bytes(uint8_t *readed_bytes, size_t *number_of_readed_bytes)
{
    uint8_t byte;
    *number_of_readed_bytes = 0;

    size_t IS_READED = 1;
    while (fread(&byte, sizeof(uint8_t), 1, stdin) == IS_READED)
    {
        readed_bytes[*number_of_readed_bytes] = byte;
        *number_of_readed_bytes = *number_of_readed_bytes + 1;
    }
}

void write_bytes(const uint8_t *bytes_to_write, const size_t number_of_bytes_to_write)
{
    for (size_t byte_writed = 0; byte_writed < number_of_bytes_to_write; byte_writed++)
    {
        fwrite(&bytes_to_write[byte_writed], sizeof(uint8_t), 1, stdout);
    }
}

uint8_t *feistel_function(const uint8_t *input_block, const uint8_t *s_box)
{
    uint8_t *output_block = (uint8_t *)malloc(HALF_BLOCK_SIZE);

    if (output_block == NULL) // memory allocation error
    {
        printf("Error allocating memory for output block\n");
        exit(1);
    }

    uint8_t index = input_block[3];
    output_block[0] = s_box[index];

    index = index + input_block[2];
    output_block[1] = s_box[index];

    index = index + input_block[1];
    output_block[2] = s_box[index];

    index = index + input_block[0];
    output_block[3] = s_box[index];

    return output_block;
}

void feistel_network(const uint8_t *block, const struct s_box *sboxes, uint8_t **cipher_block)
{
    uint8_t *L = (uint8_t *)malloc(HALF_BLOCK_SIZE);
    uint8_t *R = (uint8_t *)malloc(HALF_BLOCK_SIZE);

    if (L == NULL || R == NULL) // memory allocation error
    {
        printf("Error allocating memory for L and/or R.\n");
        exit(1);
    }

    // Split the block in two halfs (L and R)
    memcpy(L, block, HALF_BLOCK_SIZE);
    memcpy(R, block + HALF_BLOCK_SIZE, HALF_BLOCK_SIZE);

    uint8_t *M = (uint8_t *)malloc(HALF_BLOCK_SIZE);
    for (int round = 0; round < NUMBER_OF_ROUNDS; round++)
    {
        // Copy the right to a temporary variable
        memcpy(M, R, HALF_BLOCK_SIZE);

        // Apply the feistel function to the right half
        uint8_t *feistel_result = feistel_function(R, sboxes[round].sbox);

        for (int index = 0; index < HALF_BLOCK_SIZE; index++)
        {
            R[index] = L[index] ^ feistel_result[index];
        }

        memcpy(L, M, HALF_BLOCK_SIZE);
    }

    // Concatenate the left and right halfs
    memcpy(*cipher_block, L, HALF_BLOCK_SIZE);
    memcpy(*cipher_block + HALF_BLOCK_SIZE, R, HALF_BLOCK_SIZE);
}

void inverse_feistel_network(const uint8_t *block, const struct s_box *sboxes, uint8_t **cipher_block)
{
    uint8_t *L = (uint8_t *)malloc(HALF_BLOCK_SIZE);
    uint8_t *R = (uint8_t *)malloc(HALF_BLOCK_SIZE);

    if (L == NULL || R == NULL) // memory allocation error
    {
        printf("Error allocating memory for L and/or R.\n");
        exit(1);
    }

    // Split the block in two halfs (L and R)
    memcpy(L, block, HALF_BLOCK_SIZE);
    memcpy(R, block + HALF_BLOCK_SIZE, HALF_BLOCK_SIZE);

    uint8_t *M = (uint8_t *)malloc(HALF_BLOCK_SIZE);
    for (int round = NUMBER_OF_ROUNDS - 1; round >= 0; round--)
    {
        // Copy the left to a temporary variable
        memcpy(M, L, HALF_BLOCK_SIZE);

        // Apply the inverse Feistel function to the right half
        uint8_t *feistel_result = feistel_function(L, sboxes[round].sbox);

        for (int index = 0; index < HALF_BLOCK_SIZE; index++)
        {
            L[index] = R[index] ^ feistel_result[index];
        }

        memcpy(R, M, HALF_BLOCK_SIZE);
    }

    // Concatenate the left and right halfs
    memcpy(*cipher_block, L, HALF_BLOCK_SIZE);
    memcpy(*cipher_block + HALF_BLOCK_SIZE, R, HALF_BLOCK_SIZE);
}

void generate_key(const uint8_t *password, uint8_t *key)
{
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, password, strlen((char *)password));
    SHA256_Final(key, &ctx);
}

void generate_single_sbox(const uint8_t *password, uint8_t *single_sbox)
{
    uint8_t key[SHA256_DIGEST_LENGTH];

    generate_key(password, key);

    for (int index = 0; index < S_BOX_SIZE; index++) // initialize the single sbox
    {
        single_sbox[index] = index;
    }

    for (int currentIndex = 0; currentIndex < S_BOX_SIZE; currentIndex++)
    {
        int newIndex = (currentIndex + key[currentIndex % SHA256_DIGEST_LENGTH]) % S_BOX_SIZE;
        uint8_t currentByte = single_sbox[currentIndex];

        single_sbox[currentIndex] = single_sbox[newIndex];
        single_sbox[newIndex] = currentByte;
    }

    memset(key, 0, SHA256_DIGEST_LENGTH);
}

void round_robin_shuffle(uint8_t *array, size_t size)
{
    uint8_t *shuffledArray = (uint8_t *)malloc(size);

    if (shuffledArray == NULL)
    {
        fprintf(stderr, "Memory allocation error\n");
        exit(1);
    }

    size_t shift = 1;
    size_t new_index = 0;
    for (size_t index = 0; index < size; index++)
    {
        shuffledArray[new_index] = array[index];
        new_index = (new_index + shift) % size;

        shift++;
        if (shift >= size)
        {
            shift = 1;
        }
    }

    for (size_t index = 0; index < size; index++)
    {
        array[index] = shuffledArray[index];
    }

    free(shuffledArray);
}

void generate_sboxes(const uint8_t *password, struct s_box *sboxes)
{
    uint8_t *single_sbox = (uint8_t *)malloc(S_BOX_SIZE);

    if (single_sbox == NULL) // memory allocation error
    {
        printf("Error allocating memory for single sbox\n");
        exit(1);
    }

    generate_single_sbox(password, single_sbox);

    uint8_t *random_bytes = (uint8_t *)malloc(NUMBER_OF_BYTES_IN_ALL_S_BOXES);

    if (random_bytes == NULL) // memory allocation error
    {
        printf("Error allocating memory for random bytes\n");
        exit(1);
    }

    for (int index = 0; index < NUMBER_OF_S_BOXES; index++)
    {
        memcpy(random_bytes + index * S_BOX_SIZE, single_sbox, S_BOX_SIZE);
    }

    free(single_sbox);

    round_robin_shuffle(random_bytes, NUMBER_OF_BYTES_IN_ALL_S_BOXES);

    for (int sbox_index = 0; sbox_index < NUMBER_OF_S_BOXES; sbox_index++)
    {
        for (int item_index = 0; item_index < S_BOX_SIZE; item_index++)
        {
            sboxes[sbox_index].sbox[item_index] = random_bytes[sbox_index * S_BOX_SIZE + item_index];
        }
    }

    free(random_bytes);
}

void add_padding(const uint8_t *plaintext, size_t plaintext_length, uint8_t **padded_plaintext, size_t *padded_length)
{
    // Calculate the new padded length
    size_t padding_bytes = BLOCK_SIZE - (plaintext_length % BLOCK_SIZE);
    *padded_length = plaintext_length + padding_bytes;

    // Allocate memory for padded_plaintext
    *padded_plaintext = (uint8_t *)malloc(*padded_length);

    if (*padded_plaintext == NULL)
    { // memory allocation error
        fprintf(stderr, "Error allocating memory for padded plaintext\n");
        exit(1);
    }

    // Copy the original plaintext
    memcpy(*padded_plaintext, plaintext, plaintext_length);

    // Add padding before the null terminator
    for (size_t i = plaintext_length; i < *padded_length; i++)
    {
        (*padded_plaintext)[i] = padding_bytes + '0';
    }
}

void remove_padding(const uint8_t *padded_plaintext, size_t padded_length, uint8_t **plaintext, size_t *plaintext_length)
{
    size_t padding_bytes = padded_plaintext[padded_length - 1] - '0';

    if (padding_bytes > BLOCK_SIZE)
    {
        padding_bytes = 0;
    }

    *plaintext_length = padded_length - padding_bytes;

    *plaintext = (uint8_t *)malloc(*plaintext_length);

    if (*plaintext == NULL) // memory allocation error
    {
        fprintf(stderr, "Error allocating memory for plaintext\n");
        exit(1);
    }

    // Copy the original plaintext including the null terminator
    memcpy(*plaintext, padded_plaintext, *plaintext_length);
}

void encrypt(const uint8_t *plaintext, const uint8_t *password, uint8_t **ciphertext, size_t *ciphertext_size)
{
    size_t plaintext_size = strlen((char *)plaintext);

    uint8_t *padded_plaintext;
    size_t padded_plaintext_size;
    add_padding(plaintext, plaintext_size, &padded_plaintext, &padded_plaintext_size);

    struct s_box *sboxes = (struct s_box *)malloc(sizeof(struct s_box) * NUMBER_OF_ROUNDS);

    if (sboxes == NULL) // memory allocation error
    {
        printf("Error allocating memory for sboxes\n");
        exit(1);
    }

    uint8_t *key = (uint8_t *)malloc(KEY_SIZE);

    if (key == NULL) // memory allocation error
    {
        printf("Error allocating memory for key\n");
        exit(1);
    }

    generate_sboxes(password, sboxes);

    *ciphertext = (uint8_t *)malloc(padded_plaintext_size);

    if (*ciphertext == NULL) // Check for memory allocation error
    {
        printf("Error allocating memory for ciphertext\n");
        exit(1);
    }

    for (size_t block_index = 0; block_index < padded_plaintext_size; block_index += BLOCK_SIZE)
    {
        uint8_t *block = (uint8_t *)malloc(BLOCK_SIZE);

        if (block == NULL) // memory allocation error
        {
            printf("Error allocating memory for block\n");
            exit(1);
        }

        memcpy(block, padded_plaintext + block_index, BLOCK_SIZE);

        uint8_t *cipher_block = (uint8_t *)malloc(BLOCK_SIZE);

        if (cipher_block == NULL) // memory allocation error
        {
            printf("Error allocating memory for cipher block\n");
            exit(1);
        }

        feistel_network(block, sboxes, &cipher_block);

        memcpy(*ciphertext + block_index, cipher_block, BLOCK_SIZE);

        // Free the block and cipher_block memory
        free(block);
        free(cipher_block);
    }

    // Update the ciphertext size
    *ciphertext_size = padded_plaintext_size;

    // Free memory
    free(sboxes);
    free(padded_plaintext);
}

void decrypt(const uint8_t *ciphertext, const size_t ciphertext_size, const uint8_t *password, uint8_t **plaintext, size_t *plaintext_size)
{
    struct s_box *sboxes = (struct s_box *)malloc(sizeof(struct s_box) * NUMBER_OF_ROUNDS);

    if (sboxes == NULL) // memory allocation error
    {
        printf("Error allocating memory for sboxes\n");
        exit(1);
    }

    uint8_t *key = (uint8_t *)malloc(KEY_SIZE);

    if (key == NULL) // memory allocation error
    {
        printf("Error allocating memory for key\n");
        exit(1);
    }

    generate_sboxes(password, sboxes);

    size_t padded_plaintext_size = ciphertext_size;
    uint8_t *padded_plaintext = (uint8_t *)malloc(padded_plaintext_size);

    if (padded_plaintext == NULL) // memory allocation error
    {
        printf("Error allocating memory for padded plaintext\n");
        exit(1);
    }

    for (size_t block_index = 0; block_index < ciphertext_size; block_index += BLOCK_SIZE)
    {
        uint8_t *block = (uint8_t *)malloc(BLOCK_SIZE);

        if (block == NULL) // memory allocation error
        {
            printf("Error allocating memory for block\n");
            exit(1);
        }

        memcpy(block, ciphertext + block_index, BLOCK_SIZE);

        uint8_t *decipher_block = (uint8_t *)malloc(BLOCK_SIZE);

        if (decipher_block == NULL) // memory allocation error
        {
            printf("Error allocating memory for decipher block\n");
            exit(1);
        }

        inverse_feistel_network(block, sboxes, &decipher_block);

        memcpy(padded_plaintext + block_index, decipher_block, BLOCK_SIZE);

        free(block);
        free(decipher_block);
    }

    remove_padding(padded_plaintext, padded_plaintext_size, plaintext, plaintext_size);

    // Free memory
    free(sboxes);
    free(padded_plaintext);
}

void ecb_encrypt(const uint8_t *plaintext, const uint8_t *password, uint8_t **ciphertext, size_t *ciphertext_size)
{
    // Declare key schedule
    DES_cblock des_key;
    DES_key_schedule schedule;
    memcpy(des_key, password, BLOCK_SIZE);

    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &schedule);

    // Add padding
    size_t plaintext_len = strlen((char *)plaintext);
    size_t padded_len;
    uint8_t *padded_plaintext;
    add_padding(plaintext, plaintext_len, &padded_plaintext, &padded_len);

    // Declare ciphertext
    *ciphertext = (uint8_t *)malloc(padded_len);
    *ciphertext_size = padded_len;

    if (*ciphertext == NULL) // memory allocation error
    {
        printf("Error allocating memory for ciphertext\n");
        exit(1);
    }

    // Encrypt
    for (size_t block_index = 0; block_index < padded_len; block_index += BLOCK_SIZE)
    {
        DES_ecb_encrypt((DES_cblock *)(padded_plaintext + block_index), (DES_cblock *)(*ciphertext + block_index), &schedule, DES_ENCRYPT);
    }

    free(padded_plaintext);
}

void ecb_decrypt(const uint8_t *ciphertext, const size_t ciphertext_size, const uint8_t *password, uint8_t **plaintext, size_t *plaintext_size)
{
    // Declare key schedule
    DES_cblock des_key;
    DES_key_schedule schedule;
    memcpy(des_key, password, BLOCK_SIZE);

    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &schedule);

    // Declare plaintext
    size_t padded_plaintext_len = ciphertext_size;
    uint8_t *padded_plaintext = (uint8_t *)malloc(padded_plaintext_len);

    if (padded_plaintext == NULL) // memory allocation error
    {
        printf("Error allocating memory for padded plaintext\n");
        exit(1);
    }

    // Decrypt
    for (size_t block_index = 0; block_index < ciphertext_size; block_index += BLOCK_SIZE)
    {
        DES_ecb_encrypt((DES_cblock *)(ciphertext + block_index), (DES_cblock *)(padded_plaintext + block_index), &schedule, DES_DECRYPT);
    }

    // Remove padding
    remove_padding(padded_plaintext, padded_plaintext_len, plaintext, plaintext_size);

    free(padded_plaintext);
}