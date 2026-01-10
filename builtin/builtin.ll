@stdout = external global ptr, align 8
@.str = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@.str.1 = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@.str.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@.str.3 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@getString.buffer = internal global [1024 x i8] zeroinitializer, align 16
@stdin = external global ptr, align 8

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @print(ptr noundef %0) #0 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr @stdout, align 8
  %4 = call i32 @fflush(ptr noundef %3)
  %5 = load ptr, ptr %2, align 8
  %6 = call i32 (ptr, ...) @printf(ptr noundef @.str, ptr noundef %5)
  ret void
}

declare i32 @fflush(ptr noundef) #1

declare i32 @printf(ptr noundef, ...) #1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @println(ptr noundef %0) #0 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr @stdout, align 8
  %4 = call i32 @fflush(ptr noundef %3)
  %5 = load ptr, ptr %2, align 8
  %6 = call i32 (ptr, ...) @printf(ptr noundef @.str.1, ptr noundef %5)
  ret void
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @printInt(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load ptr, ptr @stdout, align 8
  %4 = call i32 @fflush(ptr noundef %3)
  %5 = load i32, ptr %2, align 4
  %6 = call i32 (ptr, ...) @printf(ptr noundef @.str.2, i32 noundef %5)
  ret void
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @printlnInt(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load ptr, ptr @stdout, align 8
  %4 = call i32 @fflush(ptr noundef %3)
  %5 = load i32, ptr %2, align 4
  %6 = call i32 (ptr, ...) @printf(ptr noundef @.str.3, i32 noundef %5)
  ret void
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local ptr @getString() #0 {
  %1 = alloca ptr, align 8
  %2 = alloca i64, align 8
  %3 = alloca ptr, align 8
  %4 = load ptr, ptr @stdin, align 8
  %5 = call ptr @fgets(ptr noundef @getString.buffer, i32 noundef 1024, ptr noundef %4)
  %6 = icmp ne ptr %5, null
  br i1 %6, label %7, label %29

7:                                                ; preds = %0
  %8 = call i64 @strlen(ptr noundef @getString.buffer) #7
  store i64 %8, ptr %2, align 8
  %9 = load i64, ptr %2, align 8
  %10 = icmp ugt i64 %9, 0
  br i1 %10, label %11, label %22

11:                                               ; preds = %7
  %12 = load i64, ptr %2, align 8
  %13 = sub i64 %12, 1
  %14 = getelementptr inbounds nuw [1024 x i8], ptr @getString.buffer, i64 0, i64 %13
  %15 = load i8, ptr %14, align 1
  %16 = sext i8 %15 to i32
  %17 = icmp eq i32 %16, 10
  br i1 %17, label %18, label %22

18:                                               ; preds = %11
  %19 = load i64, ptr %2, align 8
  %20 = sub i64 %19, 1
  %21 = getelementptr inbounds nuw [1024 x i8], ptr @getString.buffer, i64 0, i64 %20
  store i8 0, ptr %21, align 1
  br label %22

22:                                               ; preds = %18, %11, %7
  %23 = load i64, ptr %2, align 8
  %24 = add i64 %23, 1
  %25 = call noalias ptr @malloc(i64 noundef %24) #8
  store ptr %25, ptr %3, align 8
  %26 = load ptr, ptr %3, align 8
  %27 = call ptr @strcpy(ptr noundef %26, ptr noundef @getString.buffer) #9
  %28 = load ptr, ptr %3, align 8
  store ptr %28, ptr %1, align 8
  br label %30

29:                                               ; preds = %0
  store ptr null, ptr %1, align 8
  br label %30

30:                                               ; preds = %29, %22
  %31 = load ptr, ptr %1, align 8
  ret ptr %31
}

declare ptr @fgets(ptr noundef, i32 noundef, ptr noundef) #1

; Function Attrs: nounwind willreturn memory(read)
declare i64 @strlen(ptr noundef) #2

; Function Attrs: nounwind allocsize(0)
declare noalias ptr @malloc(i64 noundef) #3

; Function Attrs: nounwind
declare ptr @strcpy(ptr noundef, ptr noundef) #4

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @getInt() #0 {
  %1 = alloca i32, align 4
  %2 = call i32 (ptr, ...) @__isoc99_scanf(ptr noundef @.str.2, ptr noundef %1)
  %3 = load i32, ptr %1, align 4
  ret i32 %3
}

declare i32 @__isoc99_scanf(ptr noundef, ...) #1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @readInt() #0 {
  %1 = call i32 @getInt()
  ret i32 %1
}

; Function Attrs: noinline noreturn nounwind optnone uwtable
define dso_local void @exit(i32 noundef %0) #5 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load ptr, ptr @stdout, align 8
  %4 = call i32 @fflush(ptr noundef %3)
  %5 = load i32, ptr %2, align 4
  call void @_Exit(i32 noundef %5) #10
  unreachable
}

; Function Attrs: noreturn nounwind
declare void @_Exit(i32 noundef) #6

; Function Attrs: noinline nounwind optnone uwtable
define dso_local ptr @from(ptr noundef %0) #0 {
  %2 = alloca ptr, align 8
  %3 = alloca i64, align 8
  %4 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %5 = load ptr, ptr %2, align 8
  %6 = call i64 @strlen(ptr noundef %5) #7
  store i64 %6, ptr %3, align 8
  %7 = load i64, ptr %3, align 8
  %8 = add i64 %7, 1
  %9 = call noalias ptr @malloc(i64 noundef %8) #8
  store ptr %9, ptr %4, align 8
  %10 = load ptr, ptr %4, align 8
  %11 = load ptr, ptr %2, align 8
  %12 = call ptr @strcpy(ptr noundef %10, ptr noundef %11) #9
  %13 = load ptr, ptr %4, align 8
  ret ptr %13
}