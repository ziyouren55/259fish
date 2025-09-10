main.cpp
 └─ uci.cpp/uci.h
     └─ search.cpp/search.h
         ├─ movegen.cpp/movegen.h
         │   ├─ bitboard.h ✔
         │   └─ magics.h
         ├─ position.cpp/position.h 整个棋局的总编码
         │   ├─ types.h
         │   ├─ bitboard.h 位棋盘，初步对棋子格子编码
         │   └─ misc.h
         ├─ evaluate.cpp/evaluate.h
         │   └─ nnue/
         │       ├─ nnue_accumulator.h/cpp
         │       │   ├─ nnue/features/half_ka_v2_hm.h/cpp 特征索引的初步生成
         │       │   └─ nnue_feature_transformer.h
         │       ├─ nnue_feature_transformer.h
         │       ├─ network.h/cpp
         │       └─ layers/
         ├─ tt.h/tt.cpp
         ├─ movepick.h/movepick.cpp
         └─ history.h

