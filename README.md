# Laporan Program: Server Multi-Klien Menggunakan Fork

## Fungsi dan Manfaat Program

### Fungsi Program
Program ini memiliki beberapa fungsi utama:
1. **Komunikasi Multi-Klien**: Server dapat mengelola komunikasi antara banyak klien menggunakan protokol TCP/IP.
2. **Broadcast Pesan**: Pesan yang dikirim oleh satu klien akan disebarkan ke semua klien lainnya yang terhubung.
3. **Identifikasi Klien**: Setiap klien diberi ID unik dan diminta memasukkan username, mempermudah pengelolaan komunikasi.
4. **Manajemen Proses Anak**: Server memanfaatkan fungsi `fork()` untuk menangani setiap klien dalam proses terpisah, menjaga isolasi antar klien.
5. **Pengelolaan Koneksi**: Server dapat menerima hingga 100 klien sekaligus, serta dapat menangani masuk dan keluarnya klien tanpa mengganggu koneksi lainnya.

### Manfaat Program
Program ini memungkinkan pengelolaan komunikasi antar klien secara efisien dan stabil melalui penggunaan mekanisme fork dan TCP/IP. Setiap klien diproses secara terpisah, mengoptimalkan kemampuan server dalam menangani banyak koneksi tanpa saling mengganggu. Program ini juga dilengkapi dengan fitur broadcast, di mana pesan yang dikirimkan oleh satu klien diteruskan ke seluruh klien yang terhubung. 

Keandalan server juga terjaga karena fitur pembersihan otomatis menggunakan sinyal SIGCHLD. Meskipun program dapat menangani hingga 100 klien secara bersamaan, ada keterbatasan dalam jumlah klien yang bisa dilayani, tergantung pada kapasitas sistem. Namun, program ini tetap efektif untuk aplikasi komunikasi berbasis jaringan yang membutuhkan komunikasi efisien dan manajemen proses yang baik.

## Pembahasan

### Program 1: chat_fork

#### Fitur Program
1. **Chat Multi-Klien dengan Fork**: Server dapat menangani banyak klien dengan proses terpisah untuk setiap klien menggunakan `fork()`.
2. **Broadcast Pesan**: Pesan dari satu klien disebarkan ke seluruh klien yang terhubung.
3. **Identitas Klien Terlihat**: Klien diminta memasukkan username yang unik saat pertama kali terhubung.
4. **Penanganan Proses Anak**: Server menggunakan sinyal SIGCHLD untuk membersihkan proses anak yang sudah selesai.
5. **Keamanan Pesan**: Fungsi broadcast dan format pesan memastikan pesan tidak melampaui batas buffer.
6. **Error Handling**: Penanganan kesalahan koneksi atau input, dengan mekanisme pemutusan atau penghentian program.

#### Kelebihan dan Kekurangan

##### Kelebihan:
1. **Paralelisme dengan Fork**: Proses terpisah untuk setiap klien memungkinkan komunikasi yang tidak saling mengganggu.
2. **Sederhana dan Mudah Dipahami**: Struktur program yang jelas dan modular.
3. **Fleksibel**: Menggunakan socket TCP yang dapat berjalan di berbagai platform.
4. **Pembersihan Proses Otomatis**: Proses anak yang terputus dapat dibersihkan secara otomatis.
5. **Dukungan Perintah**: Perintah seperti `/exit` memungkinkan pengguna keluar dengan mudah dari sesi.

##### Kekurangan:
1. **Overhead Proses**: Penggunaan `fork()` untuk setiap klien menyebabkan konsumsi sumber daya yang lebih tinggi dibandingkan pendekatan berbasis thread.
2. **Keterbatasan Kapasitas**: Bergantung pada kapasitas sistem, server hanya bisa menangani hingga 100 klien.
3. **Keamanan Terbatas**: Tidak ada autentikasi pengguna atau enkripsi, membuatnya tidak cocok untuk komunikasi sensitif.
4. **Kekurangan Fitur**: Tidak ada pesan privat atau log chat.
5. **Keandalan**: Jika server mati, semua klien terputus dan tidak ada mekanisme pemulihan otomatis.

#### Hasil Pengujian

1. **Pengujian Konektivitas**: Klien dapat terhubung dan server akan meminta username. Setelah username dimasukkan, server akan mengonfirmasi bahwa klien berhasil bergabung.
2. **Pengujian Beban**: Server dapat menangani hingga 100 klien secara bersamaan. Ketika lebih dari 100 klien terhubung, server menampilkan pesan "server is full" dan tidak menerima koneksi lebih lanjut.
3. **Pengujian Fungsionalitas**: Klien dapat mengirim pesan, yang akan diterima oleh klien lainnya. Perintah `/exit` dapat digunakan untuk keluar dari sesi dan memutus koneksi.

---

### Program 2: server-chatting

#### Fitur Program
1. **Server Multi-Klien Berbasis Fork**: Server dapat menangani beberapa klien menggunakan proses terpisah.
2. **Pemrosesan Pesan Klien**: Server mengembalikan pesan yang diterima dari klien melalui mekanisme echo.
3. **Identitas Klien**: Klien diberi ID unik menggunakan counter yang dilindungi mutex.
4. **Pemutusan Koneksi**: Klien dapat mengakhiri sesi dengan mengirimkan perintah "exit".
5. **Signal Handling**: Server menangani proses anak yang selesai menggunakan sinyal SIGCHLD.
6. **Kemampuan Menangani Banyak Koneksi**: Server dapat menangani banyak koneksi secara bersamaan.

#### Kelebihan dan Kekurangan

##### Kelebihan:
1. **Pemrosesan Paralel**: Server dapat menangani banyak klien secara bersamaan tanpa mengganggu satu sama lain.
2. **Struktur Modular**: Program diorganisasi dengan baik sehingga mudah untuk dipelihara.
3. **Manajemen Proses Otomatis**: Proses anak dibersihkan secara otomatis setelah selesai.
4. **Komunikasi Sederhana**: Server dapat mengembalikan pesan ke klien (echo).
5. **Penggunaan Mutex**: Counter klien terlindungi dalam lingkungan multi-proses atau multi-thread.

##### Kekurangan:
1. **Overhead Proses Tinggi**: Setiap klien menciptakan proses baru, yang lebih memakan sumber daya dibandingkan pendekatan berbasis thread.
2. **Tidak Ada Pesan Broadcast**: Server hanya dapat mengembalikan pesan dari klien tanpa kemampuan mengirim pesan ke klien lain.
3. **Keamanan Terbatas**: Tidak ada autentikasi atau enkripsi, tidak cocok untuk komunikasi yang membutuhkan privasi.
4. **Skalabilitas Terbatas**: Bergantung pada jumlah proses yang bisa diterima oleh sistem operasi.

#### Hasil Pengujian

1. **Pengujian Konektivitas**: Klien dapat terhubung dan menerima respons dari server setelah mengirimkan pesan.
2. **Pengujian Beban**: Server dapat menangani hingga 100 klien sekaligus. Pengujian dengan 150 klien menunjukkan server masih mampu menangani koneksi meskipun melampaui batas maksimum yang ditentukan.
3. **Pengujian Fungsionalitas**: Klien dapat mengirimkan pesan dan menerima balasan (echo) dari server.

---

## Kesimpulan
Program ini memberikan solusi efektif untuk komunikasi berbasis jaringan dengan kemampuan menangani banyak klien menggunakan `fork()`. Dengan fitur broadcast dan penanganan proses anak otomatis, server dapat menangani komunikasi antar klien dengan baik. Meskipun ada beberapa keterbatasan terkait jumlah klien dan konsumsi sumber daya, program ini cukup handal untuk aplikasi sederhana yang membutuhkan komunikasi antar banyak pengguna.

---

**Catatan**: Anda dapat menguji dan menjalankan server dengan skrip yang disediakan untuk simulasi banyak klien dan memastikan fungsionalitas program berjalan dengan baik.
