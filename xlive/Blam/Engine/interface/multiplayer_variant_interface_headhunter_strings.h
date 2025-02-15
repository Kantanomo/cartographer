#pragma once
#include "main/game_preferences.h"
#include "saved_games/game_variant.h"

const static wchar_t* g_multiplayer_variant_interface_headhunter_parameter_title_strings[k_language_count][k_multiplayer_variant_headhunter_parameter_count] =
{
    // English
    {
        L"Moving Bin",
        L"Point Multiplier",
        L"Suicide Point Loss",
        L"Death Point Loss",
        L"Uncontested Bin",
        L"Speed With Heads",
        L"Max Heads Carried"
    },
    // Japanese
    {
        L"移動ビン",
        L"得点倍率",
        L"自殺による減点",
        L"死亡による減点",
        L"競争なしビン",
        L"頭部保持時の速度",
        L"最大保持頭数"
    },
    // German
    {
        L"Bewegliche Tonne",
        L"Punkte-Multiplikator",
        L"Punkteverlust bei Selbstmord",
        L"Punkteverlust bei Tod",
        L"Unangefochtene Tonne",
        L"Geschwindigkeit beim Tragen von Köpfen",
        L"Maximal getragene Köpfe"
    },
    // French
    {
        L"Bac mobile",
        L"Multiplicateur de points",
        L"Perte de points en cas de suicide",
        L"Perte de points en cas de mort",
        L"Bac non contesté",
        L"Vitesse en transportant des têtes",
        L"Nombre maximal de têtes transportées"
    },
    // Spanish
    {
        L"Contenedor móvil",
        L"Multiplicador de puntos",
        L"Pérdida de puntos por suicidio",
        L"Pérdida de puntos por muerte",
        L"Contenedor sin competencia",
        L"Velocidad al transportar cabezas",
        L"Máximo de cabezas transportadas"
    },
    // Italian
    {
        L"Contenitore mobile",
        L"Moltiplicatore di punti",
        L"Perdita di punti per suicidio",
        L"Perdita di punti per morte",
        L"Contenitore non contestato",
        L"Velocità durante il trasporto delle teste",
        L"Numero massimo di teste trasportate"
    },
    // Korean
    {
        L"이동식 통",
        L"점수 배수",
        L"자살 시 점수 감소",
        L"사망 시 점수 감소",
        L"경쟁 없는 통",
        L"머리 보유 시 속도",
        L"최대 보유 머리 수"
    },
    // Chinese
    {
        L"移动箱",
        L"得分乘数",
        L"自杀扣分",
        L"死亡扣分",
        L"无竞争箱",
        L"携带头颅时的速度",
        L"最大携带头颅数"
    },
    // Portuguese
    {
        L"Caixa móvel",
        L"Multiplicador de pontos",
        L"Perda de pontos por suicídio",
        L"Perda de pontos por morte",
        L"Caixa sem competição",
        L"Velocidade ao transportar cabeças",
        L"Máximo de cabeças transportadas"
    }
};

const static wchar_t* g_multiplayer_variant_interface_headhunter_speed_with_head_strings[k_language_count][k_ctf_engine_player_speed_count] =
{
    // English
    {
        L"Slow",
        L"Normal",
        L"Fast"
    },
    // Japanese
    {
        L"遅い",
        L"普通",
        L"速い"
    },
    // German
    {
        L"Langsam",
        L"Normal",
        L"Schnell"
    },
    // French
    {
        L"Lent",
        L"Normal",
        L"Rapide"
    },
    // Spanish
    {
        L"Lento",
        L"Normal",
        L"Rápido"
    },
    // Italian
    {
        L"Lento",
        L"Normale",
        L"Veloce"
    },
    // Korean
    {
        L"느림",
        L"보통",
        L"빠름"
    },
    // Chinese
    {
        L"慢",
        L"正常",
        L"快"
    },
    // Portuguese
    {
        L"Lento",
        L"Normal",
        L"Rápido"
    }
};

const static wchar_t* g_multiplayer_variant_interface_headhunter_max_heads_carried_strings[k_language_count][k_headhunter_max_heads_carried_count] =
{
    // English
    {
        L"None",
        L"1",
        L"5",
        L"10"
    },
    // Japanese
    {
        L"なし",
        L"1",
        L"5",
        L"10"
    },
    // German
    {
        L"Keine",
        L"1",
        L"5",
        L"10"
    },
    // French
    {
        L"Aucun",
        L"1",
        L"5",
        L"10"
    },
    // Spanish
    {
        L"Ninguno",
        L"1",
        L"5",
        L"10"
    },
    // Italian
    {
        L"Nessuno",
        L"1",
        L"5",
        L"10"
    },
    // Korean
    {
        L"없음",
        L"1",
        L"5",
        L"10"
    },
    // Chinese
    {
        L"无",
        L"1",
        L"5",
        L"10"
    },
    // Portuguese
    {
        L"Nenhum",
        L"1",
        L"5",
        L"10"
    }
};
