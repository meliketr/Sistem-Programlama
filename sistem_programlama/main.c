#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void print_usage() {
    printf("Kullanim:\n");
    printf("  Arsivlemek icin:  tarsau -b <dosyalar...> -o <arsiv_adi.sau>\n");
    printf("  Arsivi acmak icin: tarsau -a <arsiv_adi.sau> [hedef_dizin]\n");
}

long check_file_suitability(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("%s giris dosyasi bulunamadi veya acilamadi!\n", filename);
        exit(1);
    }
    long size = 0;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        size++;
        if (ch < 0 || ch > 127) {
            printf("%s giris dosyasinin formati uyumsuzdur!\n", filename); 
            fclose(file);
            exit(0); 
        }
    }
    fclose(file);
    return size;
}

void create_archive(const char *output_filename, char *input_files[], int file_count) {
    char header[5000] = ""; 
    char temp[256];

    for (int i = 0; i < file_count; i++) {
        struct stat st;
        if (stat(input_files[i], &st) != 0) {
            printf("Hata: Dosya bilgileri okunamadi: %s\n", input_files[i]);
            exit(1);
        }
        int permissions = st.st_mode & 0777; 
        long size = st.st_size;

        sprintf(temp, "|%s,%o,%ld", input_files[i], permissions, size); 
        strcat(header, temp);

        if (i < file_count - 1) {
            strcat(header, " "); 
        }
    }

    int header_bytes_size = 10 + strlen(header);
    FILE *archive = fopen(output_filename, "w");
    if (archive == NULL) {
        printf("Hata: Arsiv dosyasi olusturulamadi!\n");
        exit(1);
    }

    fprintf(archive, "%010d", header_bytes_size); 
    fprintf(archive, "%s", header);

    for (int i = 0; i < file_count; i++) {
        FILE *infile = fopen(input_files[i], "r");
        if (infile == NULL) continue;
        int ch;
        while ((ch = fgetc(infile)) != EOF) { 
            fputc(ch, archive);
        }
        fclose(infile);
    }
    fclose(archive);
    printf("Dosyalar birlesitirildi.\n"); 
}

void extract_archive(const char *archive_name, const char *target_dir) {
    FILE *archive = fopen(archive_name, "r");
    if (archive == NULL) {
        printf("Arşiv dosyası uygunsuz bozuk!\n"); 
        exit(1);
    }

    char size_str[11];
    if (fread(size_str, 1, 10, archive) != 10) { 
        printf("Arşiv dosyası uygunsuz bozuk!\n"); 
        fclose(archive);
        exit(1);
    }
    size_str[10] = '\0';
    int header_size = atoi(size_str);

    if (header_size <= 10) {
        printf("Arşiv dosyası uygunsuz bozuk!\n"); 
        fclose(archive);
        exit(1);
    }

    int header_content_size = header_size - 10;
    char *header_content = malloc(header_content_size + 1);
    fread(header_content, 1, header_content_size, archive);
    header_content[header_content_size] = '\0';

    if (strcmp(target_dir, ".") != 0) {
        mkdir(target_dir, 0777); 
    }

    char *header_copy = strdup(header_content);
    char *file_record = strtok(header_copy, " "); 

    char file_names[32][256];
    int file_modes[32];
    long file_sizes[32];
    int extracted_file_count = 0;

    while (file_record != NULL && extracted_file_count < 32) {
        if (file_record[0] == '|') {
            file_record++;
        }

        char name[256];
        char mode_str[20];
        long size;
        
        sscanf(file_record, "%[^,],%[^,],%ld", name, mode_str, &size); 
        strcpy(file_names[extracted_file_count], name);
        file_modes[extracted_file_count] = strtol(mode_str, NULL, 8);
        file_sizes[extracted_file_count] = size;
        
        extracted_file_count++;
        file_record = strtok(NULL, " ");
    }
    free(header_copy);
    free(header_content);

    for (int i = 0; i < extracted_file_count; i++) {
        char full_path[512];
        if (strcmp(target_dir, ".") != 0) {
            sprintf(full_path, "%s/%s", target_dir, file_names[i]);
        } else {
            sprintf(full_path, "%s", file_names[i]);
        }

        FILE *outfile = fopen(full_path, "w");
        if (outfile == NULL) {
            printf("Hata: %s dosyasi olusturulamadi!\n", full_path);
            continue;
        }

        for (long j = 0; j < file_sizes[i]; j++) {
            int ch = fgetc(archive);
            if (ch != EOF) {
                fputc(ch, outfile);
            }
        }
        fclose(outfile);

        chmod(full_path, file_modes[i]); 
    }

    fclose(archive);
    if (strcmp(target_dir, ".") != 0) {
        printf("%s dizininde dosyalar acildi.\n", target_dir); 
    } else {
        printf("Dosyalar gecerli dizinde acildi.\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        if (argc < 3) {
            printf("Hata: Arsivlenecek dosya belirtilmedi!\n");
            return 1;
        }
        
        char *output_filename = "a.sau"; // Varsayılan değer 
        int file_count = 0;
        char *input_files[32]; 

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    output_filename = argv[i + 1]; // 
                    i++;
                } else {
                    // -o'dan sonra veri yoksa veya baska parametre basliyorsa varsayilani kullan 
                    output_filename = "a.sau"; 
                }
            } else {
                if (file_count < 32) { 
                    input_files[file_count++] = argv[i];
                } else {
                    printf("Hata: Giris dosyasi sayisi en fazla 32 olabilir.\n"); 
                    return 1;
                }
            }
        }

        long total_size = 0;
        for (int i = 0; i < file_count; i++) {
            long file_size = check_file_suitability(input_files[i]);
            total_size += file_size;
        }

        long max_allowed_size = 200 * 1024 * 1024; 
        if (total_size > max_allowed_size) { 
            printf("Hata: Giris dosyalarinin toplam boyutu 200 MB'i gecemez!\n"); 
            return 1;
        }
        create_archive(output_filename, input_files, file_count);
    }
    else if (strcmp(argv[1], "-a") == 0) {
        if (argc < 3 || argc > 4) { 
            printf("Arşiv dosyası uygunsuz bozuk!\n"); 
            return 1;
        }
        char *archive_name = argv[2]; 
        char *target_dir = (argc == 4) ? argv[3] : "."; 

        int len = strlen(archive_name);
        if (len < 5 || strcmp(&archive_name[len - 4], ".sau") != 0) { 
            printf("Arşiv dosyası uygunsuz bozuk!\n"); 
            return 1;
        }

        extract_archive(archive_name, target_dir);
    }
    else {
        print_usage();
        return 1;
    }
    return 0;
}