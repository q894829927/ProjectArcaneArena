# Project Arcane Arena — Content 全量资产清单与目标目录

> 生成基线：`develop@5f224a9d0505`  
> 扫描范围：Git 仓库中的 `Content/` 全部文件。  
> 当前纳入文件：**748**；按用户要求排除的第三方目录文件：**2251**。  
> Git tree `truncated=false`，因此本文清单覆盖该基线下所有被纳入的 Content 文件，而不是抽样结果。

本文用于两件事：

1. 记录迁移前 **当前真实目录与全部资产/文件**。
2. 给出迁移完成后的 **分类目标目录**，保证后续修改 C++ / Config / Python / SoftObjectPath 等硬编码路径时有统一依据。

路径迁移规则和状态仍以 `Docs/ASSET_PATH_MIGRATION.md` 为准；本文是其**全量文件级目录附件**。

## 0. 明确排除

以下目录按要求完全不写入当前树和目标树，也不参与迁移规划：

```text
/Game/SlashTrail_SoftTofu
/Game/ParagonMuriel
/Game/CombatMagicAnims
/Game/wukongManny
/Game/Characters/Mannequins
```

注意：`Stylized_Spruce_Forest` 与 `LevelPrototyping` 不在本次排除名单内，因此本文仍完整记录；它们在目标规划中默认**保留原位**，不做项目命名空间迁移。

## 1. 汇总

| 分类 | 文件数 | 处理方式 |
|---|---:|---|
| 项目资产迁移到 `/Game/ProjectArcaneArena` | 271 | 通过 Unreal Editor Content Browser Move |
| 保留原位 | 259 | `Stylized_Spruce_Forest`、`LevelPrototyping` |
| Python 脚本移出 Content | 54 | 目标 `Scripts/Python` |
| 文档移出 Content | 1 | 目标 `Docs/Archive` |
| UE 自动管理文件 | 162 | `__ExternalActors__ / __ExternalObjects__`，禁止手工移动 |
| 待人工确认 | 1 | 未知/特殊文件，迁移前先确认用途 |
| **合计** | **748** | 与当前全量扫描一致 |

## 2. 当前目录：全部资产与文件

以下目录树是迁移前基线，包含本次范围内全部 748 个文件。

```text
└─ Content/
   ├─ __ExternalActors__/
   │  ├─ Tests/
   │  │  └─ Overload/
   │  │     └─ Lvl_OverloadTest/
   │  │        ├─ 0/
   │  │        │  ├─ 8Z/
   │  │        │  │  └─ H6ZQF348KJK5BRINYIW8QN.uasset
   │  │        │  ├─ C7/
   │  │        │  │  └─ 6U3UDM4A0XVKST1SOAGTCG.uasset
   │  │        │  ├─ DS/
   │  │        │  │  └─ XK2510RKNZB6T39GCX1XZW.uasset
   │  │        │  ├─ EK/
   │  │        │  │  ├─ 6LU3U94665QLZDXUMYM4JL.uasset
   │  │        │  │  └─ IM500RL6AIKCHLCFCM9RQC.uasset
   │  │        │  ├─ JV/
   │  │        │  │  └─ E3TDY64OW6BETBTG7NORJ0.uasset
   │  │        │  └─ WV/
   │  │        │     └─ 5ZLCB3WUW5FRN1YZ9BCFBU.uasset
   │  │        ├─ 1/
   │  │        │  ├─ 5R/
   │  │        │  │  └─ WW6OIM344VFJ6P7EJDJ3IY.uasset
   │  │        │  ├─ 6B/
   │  │        │  │  └─ N7MUYT5O0TX3T9IA15I3D4.uasset
   │  │        │  ├─ CY/
   │  │        │  │  └─ OUVS3O6RAKAMK58262W218.uasset
   │  │        │  ├─ MO/
   │  │        │  │  └─ LJIZECXMH50M5ITWRP0NXY.uasset
   │  │        │  └─ S3/
   │  │        │     └─ JLV5IBD63NWSDEW9HFAIQI.uasset
   │  │        ├─ 2/
   │  │        │  ├─ 4P/
   │  │        │  │  └─ ETAA36PEZEU5FF8BDIHVJU.uasset
   │  │        │  ├─ 62/
   │  │        │  │  └─ FBXRV2PDUCOU945NYEGAA8.uasset
   │  │        │  ├─ 76/
   │  │        │  │  └─ 1FZQURUFYWERG3SCUHIY22.uasset
   │  │        │  ├─ M2/
   │  │        │  │  └─ QVK6V1XNZCNWO5IY7MT3Z8.uasset
   │  │        │  └─ V0/
   │  │        │     └─ K654TGT3PG38XSH4NZKKFL.uasset
   │  │        ├─ 3/
   │  │        │  ├─ 2O/
   │  │        │  │  └─ JRU7W4XKPRFH3MW8RG0JHV.uasset
   │  │        │  ├─ 9V/
   │  │        │  │  └─ NLJQ4K8M0QSRJVSKY2S3F9.uasset
   │  │        │  ├─ AR/
   │  │        │  │  └─ T9USQE2PHZD3T21PJDZWV0.uasset
   │  │        │  ├─ H4/
   │  │        │  │  └─ PUDLBMGNM2RW01K3DBC5T6.uasset
   │  │        │  ├─ HN/
   │  │        │  │  └─ USNRRATM8ZL8N5CEYMXSW7.uasset
   │  │        │  ├─ J4/
   │  │        │  │  └─ 27HJLR4JPU1U53NICLJ9KE.uasset
   │  │        │  ├─ NN/
   │  │        │  │  └─ DF52BURN86I8DBZW2VI42L.uasset
   │  │        │  ├─ UJ/
   │  │        │  │  └─ 9WNHYBWMWII4GFMABEODYE.uasset
   │  │        │  └─ UY/
   │  │        │     └─ 93DUC5KRF4MS9DP87SNV4O.uasset
   │  │        ├─ 4/
   │  │        │  ├─ 19/
   │  │        │  │  └─ TBLK073YDD75TF6ZKDAJKN.uasset
   │  │        │  ├─ 6O/
   │  │        │  │  └─ Y4ZH1ZZNG8D3AKZHLA9IVG.uasset
   │  │        │  ├─ 8R/
   │  │        │  │  └─ SRSAAG01H6FOGT286QGFNT.uasset
   │  │        │  ├─ AO/
   │  │        │  │  └─ 57LXWY75EEXGZYTR4L8Y01.uasset
   │  │        │  ├─ BK/
   │  │        │  │  └─ J4EANRH1P94AFYQ6ZUH2K5.uasset
   │  │        │  ├─ VL/
   │  │        │  │  └─ 611N8VKM3A75RVR6DSHCPL.uasset
   │  │        │  └─ XH/
   │  │        │     └─ 5XF2E3EMCOCC4FUCC2KQK6.uasset
   │  │        ├─ 5/
   │  │        │  ├─ 0C/
   │  │        │  │  └─ KR176K1WZ6C4RURLD9ROXX.uasset
   │  │        │  ├─ 89/
   │  │        │  │  └─ PF46Z417FJ6XN3E92H04NV.uasset
   │  │        │  ├─ 8D/
   │  │        │  │  └─ 0MSFFQWF47730HH30P0REO.uasset
   │  │        │  ├─ LH/
   │  │        │  │  └─ XIOU34F86W42NERL0R3LHU.uasset
   │  │        │  ├─ OC/
   │  │        │  │  └─ AYMYU2MHVBKJF92TPCL514.uasset
   │  │        │  ├─ RO/
   │  │        │  │  └─ GYRR4M43NJM46YTIHPFCHR.uasset
   │  │        │  ├─ TB/
   │  │        │  │  └─ U6OFGEFT5FGQQS0QIMLEG6.uasset
   │  │        │  └─ TP/
   │  │        │     └─ K3BA527LFP61T9H4PJ3GYK.uasset
   │  │        ├─ 6/
   │  │        │  ├─ 25/
   │  │        │  │  └─ 6AE1JN3RA9W2RWBDNLCNBF.uasset
   │  │        │  ├─ 9Z/
   │  │        │  │  └─ RRJ26XDWVFAAAJMD2LIK9J.uasset
   │  │        │  ├─ B7/
   │  │        │  │  └─ FHMJS25AL5PMGKJLJWNJAX.uasset
   │  │        │  ├─ C7/
   │  │        │  │  └─ 923ME62ZN17ABEA7WYX1FF.uasset
   │  │        │  ├─ K5/
   │  │        │  │  └─ HNQDMW6UQZ2SYZOSVP34OW.uasset
   │  │        │  ├─ O0/
   │  │        │  │  └─ ETM74LR4BWLSUNS1Z3TXDJ.uasset
   │  │        │  ├─ P2/
   │  │        │  │  └─ UXEOWPXHE9DR9F5EYIMXPM.uasset
   │  │        │  ├─ YT/
   │  │        │  │  └─ S6NR5P38FB2KCCM9CHGXO4.uasset
   │  │        │  └─ YZ/
   │  │        │     └─ 70M9BKR1T9ZH3O9VHHMKDC.uasset
   │  │        ├─ 7/
   │  │        │  ├─ E3/
   │  │        │  │  └─ IBAFOCXDSRYRWJ9HLANY3C.uasset
   │  │        │  ├─ KD/
   │  │        │  │  └─ A6V8ZG15ALWXASN1F232XG.uasset
   │  │        │  └─ NC/
   │  │        │     └─ LK2R14LB94DE2EOY4LBH9O.uasset
   │  │        ├─ 8/
   │  │        │  ├─ GQ/
   │  │        │  │  └─ QU3JR04TVNWHWGPFWG9CA0.uasset
   │  │        │  ├─ IS/
   │  │        │  │  └─ 0195SD4HDVHT1RBL827QAQ.uasset
   │  │        │  ├─ OD/
   │  │        │  │  └─ CRI3RU9K3J7T7AGAHU9QVE.uasset
   │  │        │  ├─ V1/
   │  │        │  │  └─ 0YERKF0OAQERVZ58LRWTFF.uasset
   │  │        │  └─ Z4/
   │  │        │     └─ 8Q3816K2SQTMDM032C9ADR.uasset
   │  │        ├─ 9/
   │  │        │  ├─ 5U/
   │  │        │  │  └─ B4SUF2HRABPSMFLHS9U7AP.uasset
   │  │        │  ├─ AB/
   │  │        │  │  └─ 0SRXH9RY80WUYIE7M36NFL.uasset
   │  │        │  ├─ GD/
   │  │        │  │  └─ SCOHWF9V440DFXTMSX0PLI.uasset
   │  │        │  ├─ HD/
   │  │        │  │  └─ XERZAOAVIBJ6IB5MK3OEBA.uasset
   │  │        │  ├─ R2/
   │  │        │  │  └─ 35D4MNX7XSI289A97TBNS9.uasset
   │  │        │  └─ UI/
   │  │        │     └─ R0GL2BRVRLDHX8IAVJE09S.uasset
   │  │        ├─ A/
   │  │        │  ├─ 07/
   │  │        │  │  └─ DCO3LJLICDI0J6M6JUBZNT.uasset
   │  │        │  ├─ R7/
   │  │        │  │  └─ 21N5B0KCA16FLLAKCNCWRS.uasset
   │  │        │  ├─ XO/
   │  │        │  │  └─ KCHEPOD7KSTMWOYCUVL80D.uasset
   │  │        │  └─ Y2/
   │  │        │     └─ JOMHOJFFZ4AYL3SGCRQQ4I.uasset
   │  │        ├─ B/
   │  │        │  ├─ 8S/
   │  │        │  │  └─ 8Z0JVOGBH5N0SHADXBSHYD.uasset
   │  │        │  ├─ B5/
   │  │        │  │  └─ UDRDGP8FMDAKJH9XEK5J32.uasset
   │  │        │  ├─ CV/
   │  │        │  │  └─ EMRSEA9V2AMZ4QTJV8ZXS5.uasset
   │  │        │  ├─ EA/
   │  │        │  │  └─ NOYU3U7LH6JQKAFB2MOGAV.uasset
   │  │        │  ├─ EO/
   │  │        │  │  └─ EMWX2HGK6U37EP4N40X7MN.uasset
   │  │        │  ├─ JW/
   │  │        │  │  └─ H5KYH0TR796PYQB2R58HQU.uasset
   │  │        │  ├─ KK/
   │  │        │  │  └─ E4G69FEBKIEJSFZNZWXT2M.uasset
   │  │        │  └─ U0/
   │  │        │     └─ S8YLRK482DKN6LDPE5D34Z.uasset
   │  │        ├─ C/
   │  │        │  ├─ 0J/
   │  │        │  │  └─ PKJX8CRJSUG3VO78YZB33Z.uasset
   │  │        │  ├─ 0O/
   │  │        │  │  └─ Y4EFXZU31XXP74B0XOEBSD.uasset
   │  │        │  ├─ 1R/
   │  │        │  │  └─ 2MBNN2YL8T6F5MX4CZ3WXU.uasset
   │  │        │  ├─ 36/
   │  │        │  │  └─ 8THEGWEFDWR45SO7TY73JA.uasset
   │  │        │  ├─ 7X/
   │  │        │  │  └─ SFT13K73RKN2LYEPJLI768.uasset
   │  │        │  ├─ 84/
   │  │        │  │  └─ S1XVYHB2IATNSATED948E4.uasset
   │  │        │  ├─ A7/
   │  │        │  │  └─ 13A5W6DL85LUTS70TM3NV3.uasset
   │  │        │  ├─ FA/
   │  │        │  │  └─ THIL6OWZUU3BJCYMKF5BNW.uasset
   │  │        │  ├─ OP/
   │  │        │  │  └─ 3ZK6G82HWOH9QWXOTNDNJ2.uasset
   │  │        │  └─ X7/
   │  │        │     └─ NMDCA2E2LG49TM7Q2VVZ6M.uasset
   │  │        ├─ D/
   │  │        │  ├─ 29/
   │  │        │  │  └─ Y51JATUN9B9IM4MY2375QX.uasset
   │  │        │  ├─ 3M/
   │  │        │  │  └─ ZJVBERW28T8M8U8D01RGO2.uasset
   │  │        │  ├─ 7R/
   │  │        │  │  └─ A8NDU2I5T20CPVB4784W3E.uasset
   │  │        │  ├─ 8S/
   │  │        │  │  └─ DZRCH7B8V4I9LA4TXZIIA7.uasset
   │  │        │  ├─ JL/
   │  │        │  │  └─ CPZC9EOS1FQTWKZY8ZJL3J.uasset
   │  │        │  ├─ KE/
   │  │        │  │  └─ NMZHTVOU58UMOED9QTCJ0O.uasset
   │  │        │  └─ PN/
   │  │        │     └─ Q9ICZMAILP63KHQ27K6N5H.uasset
   │  │        ├─ E/
   │  │        │  ├─ 28/
   │  │        │  │  └─ NPH3CPP0ZL0PGP0178SZ8O.uasset
   │  │        │  ├─ D0/
   │  │        │  │  └─ 7J7KBII5HBKWHLJSZXITQS.uasset
   │  │        │  ├─ EJ/
   │  │        │  │  └─ IM3AZMUKQRB5S7G4T4L556.uasset
   │  │        │  ├─ MS/
   │  │        │  │  └─ NOQZSU27V5CVN4380HCQZE.uasset
   │  │        │  └─ ZR/
   │  │        │     └─ JW4OFGAZLNSH5URGPCKL05.uasset
   │  │        └─ F/
   │  │           ├─ 0T/
   │  │           │  └─ KEGJKVZ214BH32QGBI873X.uasset
   │  │           ├─ 25/
   │  │           │  └─ 437GFIR6BKYTKHJESNUOVI.uasset
   │  │           ├─ 4U/
   │  │           │  └─ OF0KJ35L96TD1NTJWVPXZF.uasset
   │  │           └─ 4X/
   │  │              └─ ASJKPD4ORAC9RSS1JZNVGB.uasset
   │  └─ TopDown/
   │     └─ Lvl_TopDown/
   │        ├─ 1/
   │        │  ├─ 4R/
   │        │  │  └─ USWRROF0YGZZGWQ2CHNHKA.uasset
   │        │  ├─ ED/
   │        │  │  └─ GP2N37NOM204TM8HSIOHDA.uasset
   │        │  ├─ H7/
   │        │  │  └─ HXHBY8XB24FN2PZ9N5HJNL.uasset
   │        │  ├─ KK/
   │        │  │  └─ FN2AKQMCUI8N4G72TP2P2F.uasset
   │        │  ├─ RQ/
   │        │  │  └─ JQBEDFO07FYQTBAZRBESFQ.uasset
   │        │  ├─ RT/
   │        │  │  └─ UXR7VJZGTACYQ5KESB48LQ.uasset
   │        │  ├─ SW/
   │        │  │  └─ XDDW2UFRQWQEHQ45U62WCG.uasset
   │        │  ├─ VX/
   │        │  │  └─ WL3LKELHSK570W7KZPMJLL.uasset
   │        │  ├─ WI/
   │        │  │  └─ 54CLCT4Y9TRDQT3H5CBBO2.uasset
   │        │  └─ Y4/
   │        │     └─ CP52WV1UMP464PD8ICZOTA.uasset
   │        ├─ 2/
   │        │  ├─ 0B/
   │        │  │  └─ PMMHST2XKRV46LOJVX1P5K.uasset
   │        │  ├─ 1W/
   │        │  │  └─ YHRY4WLD8BTNQ6QGV0P0HD.uasset
   │        │  ├─ 33/
   │        │  │  └─ SKUR0WWL7JT7C3OXZMWAQS.uasset
   │        │  └─ EU/
   │        │     └─ C5FXL3H56VG2BGRBVI945A.uasset
   │        ├─ 3/
   │        │  ├─ 3W/
   │        │  │  └─ KVKK3JZ9YOQDKZN4SA819U.uasset
   │        │  └─ DB/
   │        │     └─ 3WAT7AKB2FD7W2F2FBB3RL.uasset
   │        ├─ 4/
   │        │  ├─ 3R/
   │        │  │  └─ FBA0Z00NHOZRLY2VS6QWS5.uasset
   │        │  ├─ AH/
   │        │  │  └─ Q0LWU54DITS9SGJR66EGIS.uasset
   │        │  ├─ EF/
   │        │  │  └─ HK90174B848TUVF24GPVDP.uasset
   │        │  └─ LA/
   │        │     └─ BYTOYHREOMLDWMZHV025W2.uasset
   │        ├─ 5/
   │        │  ├─ FO/
   │        │  │  └─ ECPD0L2VGZ9HAW86J7D07O.uasset
   │        │  ├─ MU/
   │        │  │  └─ RF58KVN4H83EF3SOHF978Y.uasset
   │        │  └─ TK/
   │        │     └─ BEGV01JEMC3BQKMHDEA7SB.uasset
   │        ├─ 6/
   │        │  ├─ 38/
   │        │  │  └─ VQ8HM2V1RTOU5IAUWY4WGW.uasset
   │        │  ├─ AP/
   │        │  │  └─ ZPKUEMBIIM2J5R0ENN58X3.uasset
   │        │  ├─ C0/
   │        │  │  └─ F9RZ6HOG7VEVHTE13KVK91.uasset
   │        │  ├─ CE/
   │        │  │  └─ DT9U6EGO7HSBDBDH26PHEU.uasset
   │        │  ├─ MF/
   │        │  │  └─ 489IW7U7P9MIAKUIULAEPS.uasset
   │        │  ├─ SX/
   │        │  │  └─ UJD585HI4TXMW9YG4YAQO1.uasset
   │        │  ├─ T0/
   │        │  │  └─ BUK2FWFXTAPKHE13W1ZVWT.uasset
   │        │  ├─ TH/
   │        │  │  └─ Z8NDYBYXW69596N30WYMU7.uasset
   │        │  └─ VW/
   │        │     └─ BJ8OS0VW6H2H0GK1FQS2F2.uasset
   │        ├─ 7/
   │        │  ├─ 4B/
   │        │  │  └─ T1PIVNI01BWDPTT6KC0IOK.uasset
   │        │  ├─ 6M/
   │        │  │  └─ QTS3CWKKBCQU2OQX281KWZ.uasset
   │        │  ├─ NA/
   │        │  │  └─ 4DMAZX2N566NQIXEI3N7ZM.uasset
   │        │  └─ PF/
   │        │     └─ I0DNH0U6BTFFYSKQYHTIC2.uasset
   │        ├─ 8/
   │        │  ├─ PY/
   │        │  │  └─ QMLKP6A3WDUNKON0OGPXW6.uasset
   │        │  └─ Y1/
   │        │     └─ 3VWCZVZL4ZW54PZAZFVD6U.uasset
   │        ├─ 9/
   │        │  ├─ OW/
   │        │  │  └─ 75DR5F3ICVIHE723FRDK10.uasset
   │        │  ├─ V9/
   │        │  │  └─ VCZ6Z3BRAFZ7RBG1Y9IVQD.uasset
   │        │  └─ XA/
   │        │     └─ AKS4QGDKLNT2ECMCM0OSYJ.uasset
   │        ├─ A/
   │        │  ├─ 11/
   │        │  │  └─ XDMMS42FJJ729ISOP9VU34.uasset
   │        │  ├─ 3I/
   │        │  │  └─ L42FC3SQPNLGCNAE52AMJA.uasset
   │        │  ├─ FM/
   │        │  │  └─ E2Z2GWJUV6DYF4T64DU12Y.uasset
   │        │  └─ UI/
   │        │     └─ FYPDNBRF80ZC1CFUDAJ8O0.uasset
   │        ├─ B/
   │        │  └─ HK/
   │        │     └─ 5C7FMPWMFNNW4N5A6KM7CJ.uasset
   │        ├─ C/
   │        │  ├─ 46/
   │        │  │  └─ WDAXNYJP9CJF9H8ENZMQZ0.uasset
   │        │  ├─ BP/
   │        │  │  └─ BUPDHPEMHC4VKWBMQ8RZHT.uasset
   │        │  ├─ CF/
   │        │  │  └─ AT4JBJMPCJ00E8TPH8BEXB.uasset
   │        │  └─ QY/
   │        │     └─ NWXGPXGH1INB3E31CV33D7.uasset
   │        ├─ D/
   │        │  ├─ 7X/
   │        │  │  └─ F3A4HCV21GTJXPK2SLR5GG.uasset
   │        │  ├─ SH/
   │        │  │  └─ PW0AQVQUKM8YU4NRQMBZNU.uasset
   │        │  └─ Y3/
   │        │     └─ PL6BFHEQZX9767CCT3NKJG.uasset
   │        └─ E/
   │           ├─ 05/
   │           │  └─ CUR46BAKJ3XVWLZ272OSZL.uasset
   │           ├─ 1J/
   │           │  └─ HRXBHJMHKGUJ9O51YEGVI7.uasset
   │           └─ P0/
   │              └─ 6BJ49D2QVGYTGKTJ12J1UB.uasset
   ├─ __ExternalObjects__/
   │  ├─ Tests/
   │  │  └─ Overload/
   │  │     └─ Lvl_OverloadTest/
   │  │        ├─ 3/
   │  │        │  └─ LE/
   │  │        │     └─ VQU1IPC97UBIPCNI3C7YOY.uasset
   │  │        └─ 4/
   │  │           └─ SE/
   │  │              └─ UV9WMKZC3Z40VOEQAWSG1P.uasset
   │  └─ TopDown/
   │     └─ Lvl_TopDown/
   │        ├─ 1/
   │        │  └─ 5F/
   │        │     └─ ACU59NVHABC7KE5P5H7QYG.uasset
   │        └─ 7/
   │           └─ XH/
   │              └─ ILKF82DAIKT5TPOED6FWR3.uasset
   ├─ Assets/
   │  └─ Pickups/
   │     ├─ HealthCrystal/
   │     │  ├─ Materials/
   │     │  │  ├─ HealthCrystal_BaseColor.uasset
   │     │  │  ├─ HealthCrystal_Emissive.uasset
   │     │  │  ├─ HealthCrystal_Normal.uasset
   │     │  │  ├─ HealthCrystal_OcclusionRoughnessMetallic.uasset
   │     │  │  ├─ M_HealthCrystal.uasset
   │     │  │  └─ MI_HealthCrystal.uasset
   │     │  └─ SM_HealthCrystal.uasset
   │     └─ Potions/
   │        ├─ Bottle/
   │        │  ├─ M_Bottle.uasset
   │        │  ├─ MI_Bottle.uasset
   │        │  ├─ Potion_low_Bottle_BaseColor.uasset
   │        │  ├─ Potion_low_Bottle_Emissive.uasset
   │        │  ├─ Potion_low_Bottle_Normal.uasset
   │        │  ├─ Potion_low_Bottle_OcclusionRoughnessMetallic.uasset
   │        │  └─ Potion_low_Bottle_Opacity.uasset
   │        ├─ Cork/
   │        │  ├─ M_Cork.uasset
   │        │  ├─ T_Cork_BaseColor.uasset
   │        │  ├─ T_Cork_Normal.uasset
   │        │  └─ T_Cork_OcclusionRoughnessMetallic.uasset
   │        ├─ Liquid/
   │        │  ├─ M_WhiteLiquid.uasset
   │        │  ├─ MI_BlueLiquid.uasset
   │        │  ├─ MI_RedLiquid.uasset
   │        │  ├─ T_WhiteLiquid_BaseColor.uasset
   │        │  ├─ T_WhiteLiquid_Emissive.uasset
   │        │  ├─ T_WhiteLiquid_Normal.uasset
   │        │  ├─ T_WhiteLiquid_OcclusionRoughnessMetallic.uasset
   │        │  └─ T_WhiteLiquid_Opacity.uasset
   │        └─ SM_PotionBottle.uasset
   ├─ Blueprints/
   │  ├─ ArenaLightningStormArea/
   │  │  └─ BP_ArenaLightningStormArea.uasset
   │  └─ DataAsset/
   │     ├─ DA_Waves_Prototype_test.uasset
   │     └─ DA_Waves_Prototype.uasset
   ├─ Boss/
   │  ├─ AI/
   │  │  ├─ BB_ArenaBoss.uasset
   │  │  ├─ BP_ArenaBossAIController.uasset
   │  │  └─ BT_ArenaBoss.uasset
   │  ├─ Animation/
   │  │  ├─ ABP_ArenaBoss.uasset
   │  │  ├─ AM_BossCharge.uasset
   │  │  ├─ AM_BossDeath.uasset
   │  │  ├─ AM_BossFireZone.uasset
   │  │  ├─ AM_BossGroundSlam.uasset
   │  │  ├─ AM_BossSummonMinions.uasset
   │  │  ├─ AS_BossCharge.uasset
   │  │  ├─ AS_BossDeath.uasset
   │  │  ├─ AS_BossFireZone.uasset
   │  │  ├─ AS_BossGroundSlam.uasset
   │  │  └─ AS_BossSummonMinions.uasset
   │  ├─ Character/
   │  │  ├─ BP_ArenaBossCharacter.uasset
   │  │  └─ SKM_ArenaBoss.uasset
   │  ├─ GAS/
   │  │  ├─ Area/
   │  │  │  └─ BP_ArenaBossFireZoneArea.uasset
   │  │  ├─ GameplayAbility/
   │  │  │  ├─ GA_BossCharge.uasset
   │  │  │  ├─ GA_BossFireZone.uasset
   │  │  │  ├─ GA_BossGroundSlam.uasset
   │  │  │  └─ GA_BossSummonMinions.uasset
   │  │  ├─ GameplayCue/
   │  │  │  ├─ GCN_BossCharge_Active.uasset
   │  │  │  ├─ GCN_BossCharge_Impact.uasset
   │  │  │  ├─ GCN_BossCharge_Telegraph.uasset
   │  │  │  ├─ GCN_BossDeath.uasset
   │  │  │  ├─ GCN_BossEnraged_Active.uasset
   │  │  │  ├─ GCN_BossFireZone_Active.uasset
   │  │  │  ├─ GCN_BossFireZone_Telegraph.uasset
   │  │  │  ├─ GCN_BossGroundSlam_Impact.uasset
   │  │  │  ├─ GCN_BossGroundSlam_Telegraph.uasset
   │  │  │  ├─ GCN_BossPhase_Transition.uasset
   │  │  │  ├─ GCN_BossSummon_Cast.uasset
   │  │  │  └─ GCN_BossSummon_Spawn.uasset
   │  │  └─ GameplayEffect/
   │  │     ├─ GE_Boss_Enrage.uasset
   │  │     ├─ GE_Boss_PlayerCountScaling.uasset
   │  │     ├─ GE_Cooldown_BossCharge.uasset
   │  │     ├─ GE_Cooldown_BossFireZone.uasset
   │  │     ├─ GE_Cooldown_BossGroundSlam.uasset
   │  │     ├─ GE_Cooldown_BossSummonMinions.uasset
   │  │     └─ GE_Init_BossAttributes.uasset
   │  └─ VFX/
   │     ├─ NS_BossCharge_Active.uasset
   │     ├─ NS_BossCharge_Impact.uasset
   │     ├─ NS_BossCharge_Telegraph.uasset
   │     ├─ NS_BossDeath.uasset
   │     ├─ NS_BossEnraged_Active.uasset
   │     ├─ NS_BossFireZone_Active.uasset
   │     ├─ NS_BossFireZone_Telegraph.uasset
   │     ├─ NS_BossGroundSlam_Impact.uasset
   │     ├─ NS_BossGroundSlam_Telegraph.uasset
   │     ├─ NS_BossPhase_Transition.uasset
   │     ├─ NS_BossSummon_Cast.uasset
   │     └─ NS_BossSummon_Spawn.uasset
   ├─ Characters/
   │  ├─ ArenaEnemy/
   │  │  ├─ ABP_ArenaEnemy.uasset
   │  │  ├─ BP_ArenaEnemyCharacter.uasset
   │  │  └─ BP_ArenaRangedEnemy.uasset
   │  └─ ArenaPlayer/
   │     └─ BP_ArenaPlayerCharacter.uasset
   ├─ Core/
   │  └─ BP_ArenaPlayerController.uasset
   ├─ Cursor/
   │  ├─ FX_Cursor.uasset
   │  ├─ M_Cursor.uasset
   │  ├─ SM_CursorMesh.uasset
   │  └─ T_Arrow.uasset
   ├─ Data/
   │  ├─ EnemyAffix/
   │  │  ├─ DA_EnemyAffix_ArcaneWarden.uasset
   │  │  ├─ DA_EnemyAffix_Frenzy.uasset
   │  │  └─ DA_EnemyAffix_Volatile.uasset
   │  ├─ Pickup/
   │  │  └─ DA_PickupDropTable_Default.uasset
   │  ├─ Upgrade/
   │  │  ├─ DA_Upgrade_AttackPower.uasset
   │  │  ├─ DA_Upgrade_CritChance.uasset
   │  │  ├─ DA_Upgrade_DashCooldown.uasset
   │  │  ├─ DA_Upgrade_DashLightningTrail.uasset
   │  │  ├─ DA_Upgrade_EnergyOnAbilityCast.uasset
   │  │  ├─ DA_Upgrade_EnergyOnCrit.uasset
   │  │  ├─ DA_Upgrade_EnergyOnKill.uasset
   │  │  ├─ DA_Upgrade_FireballBurning.uasset
   │  │  ├─ DA_Upgrade_FireballDamage.uasset
   │  │  ├─ DA_Upgrade_LightningStormDamage.uasset
   │  │  ├─ DA_Upgrade_LightningStormShocked.uasset
   │  │  ├─ DA_Upgrade_MaxHealth.uasset
   │  │  ├─ DA_Upgrade_MoveSpeed.uasset
   │  │  ├─ DA_Upgrade_Overload.uasset
   │  │  ├─ DA_Upgrade_ShieldAmount.uasset
   │  │  └─ DA_Upgrade_ShieldBreakBlast.uasset
   │  └─ Weapon/
   │     ├─ DA_Weapon_ArcaneBolt_Fast.uasset
   │     ├─ DA_Weapon_ArcaneBolt.uasset
   │     └─ DA_Weapon_Shotgun.uasset
   ├─ GameMode/
   │  └─ BP_ArenaGameMode.uasset
   ├─ GAS/
   │  ├─ Area/
   │  │  └─ BP_ArenaDashTrailArea.uasset
   │  ├─ DamageFeedback/
   │  │  ├─ CS_DamageHeavy.uasset
   │  │  ├─ CS_DamageLight.uasset
   │  │  ├─ CS_DamageMedium.uasset
   │  │  ├─ CS_ShieldBreak.uasset
   │  │  ├─ M_ArenaHitFlashOverlay.uasset
   │  │  └─ SA_ArenaHitFeedback.uasset
   │  ├─ GameplayAbility/
   │  │  ├─ GA_BasicAttack.uasset
   │  │  ├─ GA_Dash.uasset
   │  │  ├─ GA_DashLightningTrail.uasset
   │  │  ├─ GA_EnemyMeleeAttack.uasset
   │  │  ├─ GA_EnemyRangedAttack.uasset
   │  │  ├─ GA_EnergyOnAbilityCast.uasset
   │  │  ├─ GA_EnergyOnCrit.uasset
   │  │  ├─ GA_EnergyOnKill.uasset
   │  │  ├─ GA_Fireball.uasset
   │  │  ├─ GA_LightningStorm.uasset
   │  │  ├─ GA_Overload.uasset
   │  │  ├─ GA_Shield.uasset
   │  │  └─ GA_ShieldBreakBlast.uasset
   │  ├─ GameplayCues/
   │  │  ├─ DurationCue/
   │  │  │  ├─ GCN_Burning_Active.uasset
   │  │  │  ├─ GCN_Dash_Active.uasset
   │  │  │  ├─ GCN_DashLightningTrail_Active.uasset
   │  │  │  ├─ GCN_LightningStorm_Active.uasset
   │  │  │  ├─ GCN_Shield_Active.uasset
   │  │  │  └─ GCN_Shocked_Active.uasset
   │  │  ├─ Elite/
   │  │  │  ├─ GCN_Elite_ArcaneWarden_Active.uasset
   │  │  │  ├─ GCN_Elite_ArcaneWarden_Pulse.uasset
   │  │  │  ├─ GCN_Elite_Frenzy_Active.uasset
   │  │  │  ├─ GCN_Elite_Frenzy_Trigger.uasset
   │  │  │  ├─ GCN_Elite_Volatile_Active.uasset
   │  │  │  ├─ GCN_Elite_Volatile_Explode.uasset
   │  │  │  └─ GCN_Elite_Volatile_Telegraph.uasset
   │  │  └─ InstaneCue/
   │  │     ├─ GCN_BasicAttack_Activate.uasset
   │  │     ├─ GCN_DamageCritical.uasset
   │  │     ├─ GCN_DamageNumber.uasset
   │  │     ├─ GCN_EnemyMelee_Activate.uasset
   │  │     ├─ GCN_Fireball_Cast.uasset
   │  │     ├─ GCN_HealthHit.uasset
   │  │     ├─ GCN_Hit_Fire.uasset
   │  │     ├─ GCN_Hit_Lightning.uasset
   │  │     ├─ GCN_Hit_Physical.uasset
   │  │     ├─ GCN_LightningStorm_Cast.uasset
   │  │     ├─ GCN_Overload_Explosion.uasset
   │  │     ├─ GCN_ShieldBreak_Burst.uasset
   │  │     ├─ GCN_ShieldBreak.uasset
   │  │     ├─ GCN_ShieldBreakHealthHit.uasset
   │  │     └─ GCN_ShieldHit.uasset
   │  ├─ GameplayEffect/
   │  │  ├─ Status/
   │  │  │  ├─ GE_Status_Burning.uasset
   │  │  │  ├─ GE_Status_OverloadLockout.uasset
   │  │  │  └─ GE_Status_Shocked.uasset
   │  │  ├─ Trigger/
   │  │  │  ├─ GE_Trigger_EnergyOnAbilityCast.uasset
   │  │  │  ├─ GE_Trigger_EnergyOnCrit.uasset
   │  │  │  └─ GE_Trigger_EnergyOnKill.uasset
   │  │  ├─ Upgrade/
   │  │  │  ├─ GE_Upgrade_AttackPower.uasset
   │  │  │  ├─ GE_Upgrade_CritChance.uasset
   │  │  │  ├─ GE_Upgrade_MaxHealth.uasset
   │  │  │  └─ GE_Upgrade_MoveSpeed.uasset
   │  │  ├─ GE_Cooldown_BasicAttack.uasset
   │  │  ├─ GE_Cooldown_Dash.uasset
   │  │  ├─ GE_Cooldown_EnemyMeleeAttack.uasset
   │  │  ├─ GE_Cooldown_EnemyRangedAttack.uasset
   │  │  ├─ GE_Cooldown_Fireball.uasset
   │  │  ├─ GE_Cooldown_LightningStorm.uasset
   │  │  ├─ GE_Cooldown_Shield.uasset
   │  │  ├─ GE_Cost_Fireball.uasset
   │  │  ├─ GE_Cost_LightningStorm.uasset
   │  │  ├─ GE_Cost_Shield.uasset
   │  │  ├─ GE_Damage.uasset
   │  │  ├─ GE_Elite_BaseAttributes.uasset
   │  │  ├─ GE_Elite_Frenzy.uasset
   │  │  ├─ GE_Init_EnemyAttributes.uasset
   │  │  ├─ GE_Init_PlayerAttributes.uasset
   │  │  ├─ GE_Shield_Grant.uasset
   │  │  ├─ GE_Shield.uasset
   │  │  └─ GE_Status_Stunned.uasset
   │  └─ Projectile/
   │     ├─ BP_ArenaEnemyProjectile.uasset
   │     └─ BP_ArenaFireballProjectile.uasset
   ├─ Items/
   │  ├─ Inventory/
   │  │  ├─ Data/
   │  │  │  ├─ DA_EnergyPotion.uasset
   │  │  │  └─ DA_HealthPotion.uasset
   │  │  ├─ Effects/
   │  │  │  └─ GE_Cooldown_ItemConsumable.uasset
   │  │  └─ Pickups/
   │  │     ├─ BP_EnergyPotionPickup.uasset
   │  │     └─ BP_HealthPotionPickup.uasset
   │  └─ Pickups/
   │     ├─ BP_EnergyPickup.uasset
   │     └─ BP_HealthPickup.uasset
   ├─ LevelPrototyping/
   │  ├─ Interactable/
   │  │  ├─ Door/
   │  │  │  ├─ Assets/
   │  │  │  │  └─ Meshes/
   │  │  │  │     ├─ SM_Door.fbx
   │  │  │  │     ├─ SM_Door.uasset
   │  │  │  │     ├─ SM_DoorFrame_Corner.fbx
   │  │  │  │     ├─ SM_DoorFrame_Corner.uasset
   │  │  │  │     ├─ SM_DoorFrame_Edge.uasset
   │  │  │  │     └─ SM_DoorFrame_Strt.fbx
   │  │  │  └─ BP_DoorFrame.uasset
   │  │  ├─ JumpPad/
   │  │  │  ├─ Assets/
   │  │  │  │  ├─ Materials/
   │  │  │  │  │  ├─ M_GradientGlow.uasset
   │  │  │  │  │  ├─ M_SimpleGlow.uasset
   │  │  │  │  │  └─ MI_GlowNT.uasset
   │  │  │  │  ├─ Meshes/
   │  │  │  │  │  ├─ SM_CircularBand.fbx
   │  │  │  │  │  ├─ SM_CircularBand.uasset
   │  │  │  │  │  ├─ SM_CircularGlow.fbx
   │  │  │  │  │  └─ SM_CircularGlow.uasset
   │  │  │  │  └─ NS_JumpPad.uasset
   │  │  │  └─ BP_JumpPad.uasset
   │  │  └─ Target/
   │  │     ├─ Assets/
   │  │     │  ├─ SM_TargetBaseMesh.fbx
   │  │     │  └─ SM_TargetBaseMesh.uasset
   │  │     └─ BP_WobbleTarget.uasset
   │  ├─ Materials/
   │  │  ├─ M_FlatCol.uasset
   │  │  ├─ M_PrototypeGrid.uasset
   │  │  ├─ MF_ProcGrid.uasset
   │  │  ├─ MI_DefaultColorway.uasset
   │  │  ├─ MI_PrototypeGrid_Gray_02.uasset
   │  │  ├─ MI_PrototypeGrid_Gray.uasset
   │  │  └─ MI_PrototypeGrid_TopDark.uasset
   │  ├─ Meshes/
   │  │  ├─ SM_ChamferCube.fbx
   │  │  ├─ SM_ChamferCube.uasset
   │  │  ├─ SM_Cube.uasset
   │  │  ├─ SM_Cylinder.fbx
   │  │  ├─ SM_Cylinder.uasset
   │  │  ├─ SM_QuarterCylinder.fbx
   │  │  ├─ SM_QuarterCylinder.uasset
   │  │  ├─ SM_Ramp.uasset
   │  │  └─ SM_SM_ChamferCube.fbx
   │  └─ Textures/
   │     └─ T_GridChecker_A.uasset
   ├─ Mass/
   │  └─ BP_MassCluster.uasset
   ├─ Niagara/
   │  ├─ NS_DashAura.uasset
   │  ├─ NS_LightningStorm.uasset
   │  ├─ NS_Shield_Muriel_BeforeLifetimeFix.uasset
   │  ├─ NS_Shield_Muriel.uasset
   │  └─ NS_ShieldAura.uasset
   ├─ Python/
   │  ├─ balance/
   │  │  └─ apply_resume_demo_balance.py
   │  ├─ boss/
   │  │  ├─ README.md
   │  │  ├─ setup_boss_charge.py
   │  │  ├─ setup_boss_decision.py
   │  │  ├─ setup_boss_fire_zone.py
   │  │  ├─ setup_boss_foundation.py
   │  │  ├─ setup_boss_phase_system.py
   │  │  ├─ setup_boss_player_scaling.py
   │  │  ├─ setup_boss_summon_minions.py
   │  │  └─ setup_boss_victory_outro.py
   │  ├─ build_assets/
   │  │  ├─ __init__.py
   │  │  ├─ arena_asset_tools.py
   │  │  ├─ configure_build_asset_links.py
   │  │  ├─ generate_actor_blueprints.py
   │  │  ├─ generate_burst_gameplay_cues.py
   │  │  ├─ generate_damage_number_gameplay_cues.py
   │  │  ├─ generate_gameplay_ability_blueprints.py
   │  │  ├─ generate_gameplay_effect_blueprints.py
   │  │  ├─ generate_looping_gameplay_cues.py
   │  │  ├─ generate_upgrade_assets.py
   │  │  ├─ README.md
   │  │  └─ setup_build_assets.py
   │  ├─ damage_feedback/
   │  │  ├─ README.md
   │  │  └─ setup_damage_feedback_polish.py
   │  ├─ elite_enemy/
   │  │  ├─ README.md
   │  │  └─ setup_elite_enemy.py
   │  ├─ inventory/
   │  │  ├─ __init__.py
   │  │  ├─ README.md
   │  │  └─ setup_inventory_items.py
   │  ├─ other/
   │  │  ├─ fix_muriel_shield_niagara_lifetime.py
   │  │  ├─ import_upgrade_icons.py
   │  │  ├─ setup_muriel_shield_cue.py
   │  │  └─ setup_wukong_dash.py
   │  ├─ overload_test/
   │  │  ├─ README.md
   │  │  └─ setup_overload_test.py
   │  ├─ pcg_lab/
   │  │  ├─ deep_diagnose.py
   │  │  ├─ diagnose_pcg.py
   │  │  ├─ enable_forest_collision.py
   │  │  ├─ fix_mesh_selector.py
   │  │  ├─ generate_pcg_lab.py
   │  │  ├─ rebuild_pcg_graph.py
   │  │  ├─ replace_forest_meshes.py
   │  │  ├─ setup_forest_lab.py
   │  │  └─ setup_pcg_lab.py
   │  ├─ pickup_items/
   │  │  ├─ __init__.py
   │  │  ├─ README.md
   │  │  └─ setup_pickup_items.py
   │  ├─ projectile_stress/
   │  │  ├─ README.md
   │  │  └─ setup_projectile_stress.py
   │  ├─ ranged_enemy/
   │  │  ├─ README.md
   │  │  └─ setup_ranged_enemy.py
   │  ├─ setup_build_assets.py
   │  ├─ setup_main_menu.py
   │  └─ setup_pickup_items.py
   ├─ Stylized_Spruce_Forest/
   │  ├─ Audio/
   │  │  ├─ SC_Day_Forest_Ambient_01.uasset
   │  │  └─ SW_Day_Forest_Ambient_01.uasset
   │  ├─ Blueprints/
   │  │  ├─ Camera_Shake/
   │  │  │  ├─ BP_MoveTurnLeftCameraShake.uasset
   │  │  │  ├─ BP_MoveTurnRightCameraShake.uasset
   │  │  │  ├─ BP_SprintCameraShake.uasset
   │  │  │  ├─ BP_TurnLeftCameraShake.uasset
   │  │  │  ├─ BP_TurnRightCameraShake.uasset
   │  │  │  └─ BP_WalkCameraShake.uasset
   │  │  ├─ BP_Interactive_Foliage.uasset
   │  │  └─ BP_Procedural_Seasons.uasset
   │  ├─ Demo/
   │  │  ├─ Game_Mode/
   │  │  │  ├─ FirstPersonBP/
   │  │  │  │  └─ Blueprints/
   │  │  │  │     ├─ STZD_FirstPersonCharacter.uasset
   │  │  │  │     ├─ STZD_FirstPersonGameMode.uasset
   │  │  │  │     └─ STZD_FirstPersonHUD.uasset
   │  │  │  ├─ Mannequin/
   │  │  │  │  ├─ Animations/
   │  │  │  │  │  ├─ ThirdPerson_AnimBP.uasset
   │  │  │  │  │  ├─ ThirdPerson_IdleRun_2D.uasset
   │  │  │  │  │  ├─ ThirdPerson_Jump.uasset
   │  │  │  │  │  ├─ ThirdPersonIdle.uasset
   │  │  │  │  │  ├─ ThirdPersonJump_End.uasset
   │  │  │  │  │  ├─ ThirdPersonJump_Loop.uasset
   │  │  │  │  │  ├─ ThirdPersonJump_Start.uasset
   │  │  │  │  │  ├─ ThirdPersonRun.uasset
   │  │  │  │  │  └─ ThirdPersonWalk.uasset
   │  │  │  │  └─ Character/
   │  │  │  │     ├─ Materials/
   │  │  │  │     │  ├─ MaterialLayers/
   │  │  │  │     │  │  ├─ ML_GlossyBlack_Latex_UE4.uasset
   │  │  │  │     │  │  ├─ ML_Plastic_Shiny_Beige_LOGO.uasset
   │  │  │  │     │  │  ├─ ML_Plastic_Shiny_Beige.uasset
   │  │  │  │     │  │  ├─ ML_SoftMetal_UE4.uasset
   │  │  │  │     │  │  ├─ T_ML_Aluminum01_N.uasset
   │  │  │  │     │  │  ├─ T_ML_Aluminum01.uasset
   │  │  │  │     │  │  ├─ T_ML_Rubber_Blue_01_D.uasset
   │  │  │  │     │  │  └─ T_ML_Rubber_Blue_01_N.uasset
   │  │  │  │     │  ├─ M_Male_Body.uasset
   │  │  │  │     │  ├─ M_UE4Man_ChestLogo.uasset
   │  │  │  │     │  └─ MI_Female_Body.uasset
   │  │  │  │     ├─ Mesh/
   │  │  │  │     │  ├─ SK_Mannequin_Female_PhysicsAsset.uasset
   │  │  │  │     │  ├─ SK_Mannequin_Female.uasset
   │  │  │  │     │  ├─ SK_Mannequin_PhysicsAsset.uasset
   │  │  │  │     │  ├─ SK_Mannequin.uasset
   │  │  │  │     │  └─ UE4_Mannequin_Skeleton.uasset
   │  │  │  │     └─ Textures/
   │  │  │  │        ├─ T_Female_Mask.uasset
   │  │  │  │        ├─ T_Female_N.uasset
   │  │  │  │        ├─ T_Male_Mask.uasset
   │  │  │  │        ├─ T_Male_N.uasset
   │  │  │  │        ├─ T_UE4Logo_Mask.uasset
   │  │  │  │        └─ T_UE4Logo_N.uasset
   │  │  │  └─ ThirdPersonBP/
   │  │  │     └─ Blueprints/
   │  │  │        ├─ STZD_ThirdPersonCharacter.uasset
   │  │  │        └─ STZD_ThirdPersonGameMode.uasset
   │  │  └─ Maps/
   │  │     ├─ STZD_Demo_01.umap
   │  │     └─ STZD_Overview.umap
   │  ├─ Landscape_Layers/
   │  │  ├─ L1_LayerInfo.uasset
   │  │  ├─ L2_LayerInfo.uasset
   │  │  ├─ L3_LayerInfo.uasset
   │  │  ├─ L4_LayerInfo.uasset
   │  │  ├─ L5_LayerInfo.uasset
   │  │  ├─ L6_LayerInfo.uasset
   │  │  └─ Remove_Procedural_LayerInfo.uasset
   │  ├─ Materials/
   │  │  ├─ Master_Materials/
   │  │  │  ├─ M_Foliage.uasset
   │  │  │  ├─ M_Landscape.uasset
   │  │  │  ├─ M_Oil_Lantern_Glass.uasset
   │  │  │  ├─ M_Oil_Lantern_Main.uasset
   │  │  │  └─ M_Water.uasset
   │  │  ├─ Material_Functions/
   │  │  │  ├─ MF_Detail_Normal.uasset
   │  │  │  ├─ MF_Foliage_Interactive.uasset
   │  │  │  ├─ MF_Foliage_Winter_SSS_Color.uasset
   │  │  │  ├─ MF_Landscape_Layer_Base.uasset
   │  │  │  ├─ MF_Opacity.uasset
   │  │  │  ├─ MF_Procedural_Seasons.uasset
   │  │  │  ├─ MF_Puddles.uasset
   │  │  │  ├─ MF_Slope_Mask.uasset
   │  │  │  ├─ MF_Wind_Ripples.uasset
   │  │  │  └─ MF_Wind.uasset
   │  │  ├─ Material_Instances/
   │  │  │  ├─ Background/
   │  │  │  │  ├─ MI_Background_01.uasset
   │  │  │  │  ├─ MI_Background_02.uasset
   │  │  │  │  └─ MI_Background_03.uasset
   │  │  │  ├─ Particles/
   │  │  │  │  ├─ MI_Leaf_Autumn.uasset
   │  │  │  │  ├─ MI_Rain.uasset
   │  │  │  │  └─ MI_Snow.uasset
   │  │  │  ├─ Rocks/
   │  │  │  │  ├─ MI_Rocks_01-03.uasset
   │  │  │  │  ├─ MI_Rocks_04-05.uasset
   │  │  │  │  └─ MI_Rocks_06-09.uasset
   │  │  │  ├─ MI_Debris.uasset
   │  │  │  ├─ MI_Lake.uasset
   │  │  │  ├─ MI_Landscape.uasset
   │  │  │  ├─ MI_Meshes.uasset
   │  │  │  ├─ MI_Plants_01.uasset
   │  │  │  ├─ MI_River.uasset
   │  │  │  ├─ MI_Road_01.uasset
   │  │  │  └─ MI_Trees.uasset
   │  │  └─ Particles/
   │  │     ├─ M_Leaf.uasset
   │  │     └─ M_Rain.uasset
   │  ├─ Meshes/
   │  │  ├─ Background/
   │  │  │  ├─ SM_Background_Landscape_01.uasset
   │  │  │  ├─ SM_Background_Landscape_02.uasset
   │  │  │  └─ SM_Background_Landscape_03.uasset
   │  │  ├─ Debris/
   │  │  │  ├─ SM_Branch_01.uasset
   │  │  │  ├─ SM_Broken_Tree_01.uasset
   │  │  │  ├─ SM_Dry_Tree_01.uasset
   │  │  │  ├─ SM_Dry_Tree_02.uasset
   │  │  │  ├─ SM_Log_01.uasset
   │  │  │  ├─ SM_Logs_01.uasset
   │  │  │  ├─ SM_Logs_02.uasset
   │  │  │  ├─ SM_Mushroom_01.uasset
   │  │  │  ├─ SM_Mushroom_02.uasset
   │  │  │  ├─ SM_Mushroom_03.uasset
   │  │  │  └─ SM_Stump_01.uasset
   │  │  ├─ Plants/
   │  │  │  ├─ SM_Blue_Plant_01.uasset
   │  │  │  ├─ SM_Blue_Plant_02.uasset
   │  │  │  ├─ SM_Bush_01.uasset
   │  │  │  ├─ SM_Dry_Bush_01.uasset
   │  │  │  ├─ SM_Dry_Bush_02.uasset
   │  │  │  ├─ SM_Flowers_01.uasset
   │  │  │  ├─ SM_Grass_01.uasset
   │  │  │  ├─ SM_Grass_02.uasset
   │  │  │  ├─ SM_Plant_01.uasset
   │  │  │  ├─ SM_Red_Plant_01.uasset
   │  │  │  ├─ SM_Spruce_Bush_01.uasset
   │  │  │  └─ SM_White_Plant_01.uasset
   │  │  ├─ Rocks/
   │  │  │  ├─ SM_Rock_01.uasset
   │  │  │  ├─ SM_Rock_02.uasset
   │  │  │  ├─ SM_Rock_03.uasset
   │  │  │  ├─ SM_Rock_04.uasset
   │  │  │  ├─ SM_Rock_05.uasset
   │  │  │  ├─ SM_Rock_06.uasset
   │  │  │  ├─ SM_Rock_07.uasset
   │  │  │  ├─ SM_Rock_08.uasset
   │  │  │  ├─ SM_Rock_09.uasset
   │  │  │  ├─ SM_Small_Rock_01.uasset
   │  │  │  ├─ SM_Small_Rock_02.uasset
   │  │  │  └─ SM_Small_Rock_03.uasset
   │  │  ├─ Trees/
   │  │  │  ├─ SM_Small_Spruce_01.uasset
   │  │  │  ├─ SM_Spruce_01.uasset
   │  │  │  ├─ SM_Spruce_02.uasset
   │  │  │  ├─ SM_Spruce_03.uasset
   │  │  │  ├─ SM_Spruce_04.uasset
   │  │  │  ├─ SM_Spruce_05.uasset
   │  │  │  ├─ SM_Spruce_06.uasset
   │  │  │  └─ SM_Spruce_07.uasset
   │  │  ├─ SM_Lake.uasset
   │  │  ├─ SM_Oil_Lantern_01.uasset
   │  │  ├─ SM_River.uasset
   │  │  └─ SM_Road_01.uasset
   │  ├─ MPC/
   │  │  └─ MPC_Global.uasset
   │  ├─ Particles/
   │  │  ├─ NS_Autumn.uasset
   │  │  ├─ NS_Rain.uasset
   │  │  └─ NS_Snow.uasset
   │  ├─ Procedural/
   │  │  ├─ Landscape_Grass_Types/
   │  │  │  ├─ LGT_Bushes.uasset
   │  │  │  ├─ LGT_Flowers.uasset
   │  │  │  ├─ LGT_Grass.uasset
   │  │  │  ├─ LGT_Peblees.uasset
   │  │  │  └─ LGT_Small_Debris.uasset
   │  │  ├─ SMF_Logs/
   │  │  │  ├─ SMF_Branch_01.uasset
   │  │  │  ├─ SMF_Broken_Tree_01.uasset
   │  │  │  ├─ SMF_Dry_Tree_01.uasset
   │  │  │  ├─ SMF_Dry_Tree_02.uasset
   │  │  │  ├─ SMF_Log_01.uasset
   │  │  │  ├─ SMF_Logs_01.uasset
   │  │  │  ├─ SMF_Logs_02.uasset
   │  │  │  └─ SMF_Stump_01.uasset
   │  │  ├─ SMF_Rocks/
   │  │  │  ├─ SMF_Rock_01.uasset
   │  │  │  ├─ SMF_Rock_02.uasset
   │  │  │  ├─ SMF_Rock_03.uasset
   │  │  │  ├─ SMF_Rock_04.uasset
   │  │  │  ├─ SMF_Rock_05.uasset
   │  │  │  ├─ SMF_Slope_Rock_04.uasset
   │  │  │  └─ SMF_Slope_Rock_05.uasset
   │  │  ├─ SMF_Trees/
   │  │  │  ├─ SMF_Small_Spruce_01.uasset
   │  │  │  ├─ SMF_Spruce_01.uasset
   │  │  │  ├─ SMF_Spruce_02.uasset
   │  │  │  ├─ SMF_Spruce_03.uasset
   │  │  │  ├─ SMF_Spruce_04.uasset
   │  │  │  ├─ SMF_Spruce_05.uasset
   │  │  │  ├─ SMF_Spruce_06.uasset
   │  │  │  └─ SMF_Spruce_07.uasset
   │  │  ├─ PFS_Big_Trees_01.uasset
   │  │  ├─ PFS_Debris_01.uasset
   │  │  ├─ PFS_Rocks_01.uasset
   │  │  ├─ PFS_Slope_Rocks_01.uasset
   │  │  └─ PFS_Small_Trees_01.uasset
   │  ├─ Textures/
   │  │  ├─ Background/
   │  │  │  ├─ 01/
   │  │  │  │  ├─ T_Background_Landscape_01_Albedo.uasset
   │  │  │  │  └─ T_Background_Landscape_01_Normal.uasset
   │  │  │  ├─ 02/
   │  │  │  │  ├─ T_Background_Landscape_02_Albedo.uasset
   │  │  │  │  └─ T_Background_Landscape_02_Normal.uasset
   │  │  │  └─ 03/
   │  │  │     ├─ T_Background_Landscape_03_Albedo.uasset
   │  │  │     └─ T_Background_Landscape_03_Normal.uasset
   │  │  ├─ FX/
   │  │  │  ├─ T_Detail_Normal.uasset
   │  │  │  ├─ T_Particle_Normal.uasset
   │  │  │  ├─ T_Particles_Albedo.uasset
   │  │  │  ├─ T_Plants_Mask.uasset
   │  │  │  └─ T_Puddle_Mask.uasset
   │  │  ├─ Landscape/
   │  │  │  ├─ Grass_01/
   │  │  │  │  ├─ T_Grass_01_Albedo.uasset
   │  │  │  │  └─ T_Grass_01_Normal.uasset
   │  │  │  ├─ Ice/
   │  │  │  │  ├─ T_Ice_Albedo.uasset
   │  │  │  │  └─ T_Ice_Normal.uasset
   │  │  │  ├─ Mud/
   │  │  │  │  ├─ T_Mud_01_Albedo.uasset
   │  │  │  │  └─ T_Mud_01_Normal.uasset
   │  │  │  ├─ Needles/
   │  │  │  │  ├─ T_Needles_01_Albedo.uasset
   │  │  │  │  └─ T_Needles_01_Normal.uasset
   │  │  │  ├─ Peebles/
   │  │  │  │  ├─ T_Peebles_01_Albedo.uasset
   │  │  │  │  └─ T_Peebles_01_Normal.uasset
   │  │  │  ├─ River_Peebles_01/
   │  │  │  │  ├─ T_River_Peebles_01_Albedo.uasset
   │  │  │  │  └─ T_River_Peebles_01_Normal.uasset
   │  │  │  ├─ Road_01/
   │  │  │  │  ├─ T_Road_01_Albedo.uasset
   │  │  │  │  └─ T_Road_01_Normal.uasset
   │  │  │  ├─ Slope/
   │  │  │  │  ├─ T_Slope_01_Albedo.uasset
   │  │  │  │  └─ T_Slope_01_Normal.uasset
   │  │  │  └─ Snow/
   │  │  │     ├─ T_Snow_Albedo.uasset
   │  │  │     └─ T_Snow_Normal.uasset
   │  │  ├─ Oil_Lantern/
   │  │  │  ├─ T_Oil_Lantern_01_Albedo.uasset
   │  │  │  ├─ T_Oil_Lantern_01_Normal.uasset
   │  │  │  └─ T_Oil_Lantern_Roughness_Metallic.uasset
   │  │  ├─ Plants/
   │  │  │  ├─ Leaf_01/
   │  │  │  │  ├─ T_Leaf_01_Albedo.uasset
   │  │  │  │  └─ T_Leaf_01_Normal.uasset
   │  │  │  └─ Plants_Assembly_01/
   │  │  │     ├─ T_Plants_Assembly_01_Albedo.uasset
   │  │  │     └─ T_Plants_Assembly_01_Normal.uasset
   │  │  ├─ Rocks/
   │  │  │  ├─ Rocks_01-03/
   │  │  │  │  ├─ T_Rocks_01-03_Albedo.uasset
   │  │  │  │  └─ T_Rocks_01-03_Normal.uasset
   │  │  │  ├─ Rocks_04-05/
   │  │  │  │  ├─ T_Rocks_04-05_Albedo.uasset
   │  │  │  │  └─ T_Rocks_04-05_Normal.uasset
   │  │  │  └─ Rocks_06-09/
   │  │  │     ├─ T_Rocks_06-09_Albedo.uasset
   │  │  │     └─ T_Rocks_06-09_Normal.uasset
   │  │  ├─ Trees/
   │  │  │  ├─ T_Spruces_Albedo_01.uasset
   │  │  │  └─ T_Spruces_Normal_01.uasset
   │  │  ├─ Water/
   │  │  │  └─ T_Water_Normal.uasset
   │  │  └─ T_Interface.uasset
   │  └─ UI/
   │     └─ WB_Interface.uasset
   ├─ Tests/
   │  ├─ Overload/
   │  │  ├─ BP_ArenaEnemy_OverloadDummy.uasset
   │  │  ├─ BP_ArenaGameMode_OverloadTest.uasset
   │  │  ├─ BP_ArenaUpgradeTestPickup.uasset
   │  │  ├─ GE_Test_OverloadDummyAttributes.uasset
   │  │  └─ Lvl_OverloadTest.umap
   │  ├─ PCG/
   │  │  ├─ forest/
   │  │  │  └─ PCG_Forest.uasset
   │  │  ├─ Lvl_ForestLab_sharedassets/
   │  │  │  ├─ LI_L1.uasset
   │  │  │  ├─ LI_L2.uasset
   │  │  │  ├─ LI_L3.uasset
   │  │  │  ├─ LI_L4.uasset
   │  │  │  ├─ LI_L5.uasset
   │  │  │  └─ LI_L6.uasset
   │  │  ├─ Lvl_ForestLab.umap
   │  │  ├─ Lvl_PCGLab.umap
   │  │  ├─ PCG_Decor_Lab_Minimal.uasset
   │  │  └─ PCG_Decor_Lab.uasset
   │  └─ Projectile/
   │     └─ BP_ArenaProjectileStressTestActor.uasset
   ├─ TopDown/
   │  ├─ Blueprints/
   │  │  ├─ BP_TopDownCharacter.uasset
   │  │  ├─ BP_TopDownController.uasset
   │  │  └─ BP_TopDownGameMode.uasset
   │  ├─ Cursor/
   │  │  ├─ FX_Cursor_Failure.uasset
   │  │  ├─ FX_Cursor_Success.uasset
   │  │  ├─ M_Cursor.uasset
   │  │  ├─ MI_Cursor_Red.uasset
   │  │  ├─ SM_CursorMesh_Red.uasset
   │  │  ├─ SM_CursorMesh.uasset
   │  │  └─ T_Arrow.uasset
   │  ├─ Input/
   │  │  ├─ Actions/
   │  │  │  ├─ IA_SetDestination_Click.uasset
   │  │  │  └─ IA_SetDestination_Touch.uasset
   │  │  └─ IMC_Default.uasset
   │  ├─ Lvl_TopDown.umap
   │  └─ MI_Colorway.uasset
   ├─ UI/
   │  ├─ Inventory/
   │  │  ├─ WBP_Inventory.uasset
   │  │  └─ WBP_InventorySlot.uasset
   │  ├─ MainMenu/
   │  │  ├─ BP_ArenaMainMenuGameMode.uasset
   │  │  ├─ BP_ArenaMainMenuPlayerController.uasset
   │  │  ├─ Lvl_MainMenu.umap
   │  │  └─ WBP_MainMenu.uasset
   │  ├─ UpgradeIcons/
   │  │  ├─ T_Upgrade_AttackPower_Icon.png
   │  │  ├─ T_Upgrade_AttackPower_Icon.uasset
   │  │  ├─ T_Upgrade_CritChance_Icon.uasset
   │  │  ├─ T_Upgrade_DashCooldown_Icon.uasset
   │  │  ├─ T_Upgrade_DashLightningTrail_Icon.uasset
   │  │  ├─ T_Upgrade_EnergyOnAbilityCast_Icon.uasset
   │  │  ├─ T_Upgrade_EnergyOnCrit_Icon.uasset
   │  │  ├─ T_Upgrade_EnergyOnKill_Icon.uasset
   │  │  ├─ T_Upgrade_FireballBurning_Icon.png
   │  │  ├─ T_Upgrade_FireballBurning_Icon.uasset
   │  │  ├─ T_Upgrade_FireballDamage_Icon.png
   │  │  ├─ T_Upgrade_FireballDamage_Icon.uasset
   │  │  ├─ T_Upgrade_LightningStormDamage_Icon.png
   │  │  ├─ T_Upgrade_LightningStormDamage_Icon.uasset
   │  │  ├─ T_Upgrade_LightningStormShocked_Icon.png
   │  │  ├─ T_Upgrade_LightningStormShocked_Icon.uasset
   │  │  ├─ T_Upgrade_MaxHealth_Icon.png
   │  │  ├─ T_Upgrade_MaxHealth_Icon.uasset
   │  │  ├─ T_Upgrade_MoveSpeed_Icon.png
   │  │  ├─ T_Upgrade_MoveSpeed_Icon.uasset
   │  │  ├─ T_Upgrade_Overload_Icon.uasset
   │  │  ├─ T_Upgrade_ShieldAmount_Icon.uasset
   │  │  └─ T_Upgrade_ShieldBreakBlast_Icon.uasset
   │  ├─ BP_ArenaDamageNumberActor.uasset
   │  ├─ WBP_DamageNumber.uasset
   │  ├─ WBP_EnemyHealthBar.uasset
   │  └─ WBP_PlayerHUD.uasset
   ├─ 3
   └─ mcp-conversation-export.md
```

## 3. 分类完成后的目标目录

下面是**手工可控文件**的最终分类树。这里同时包含：
- 正式项目资产；
- 保留原位的包；
- 移出 Content 的 Python / Docs 文件。

不包含 `__ExternalActors__ / __ExternalObjects__` 的目标路径，因为这两类必须由 Unreal Engine 随地图管理，详见第 4 节。

```text
├─ Content/
│  ├─ LevelPrototyping/
│  │  ├─ Interactable/
│  │  │  ├─ Door/
│  │  │  │  ├─ Assets/
│  │  │  │  │  └─ Meshes/
│  │  │  │  │     ├─ SM_Door.fbx
│  │  │  │  │     ├─ SM_Door.uasset
│  │  │  │  │     ├─ SM_DoorFrame_Corner.fbx
│  │  │  │  │     ├─ SM_DoorFrame_Corner.uasset
│  │  │  │  │     ├─ SM_DoorFrame_Edge.uasset
│  │  │  │  │     └─ SM_DoorFrame_Strt.fbx
│  │  │  │  └─ BP_DoorFrame.uasset
│  │  │  ├─ JumpPad/
│  │  │  │  ├─ Assets/
│  │  │  │  │  ├─ Materials/
│  │  │  │  │  │  ├─ M_GradientGlow.uasset
│  │  │  │  │  │  ├─ M_SimpleGlow.uasset
│  │  │  │  │  │  └─ MI_GlowNT.uasset
│  │  │  │  │  ├─ Meshes/
│  │  │  │  │  │  ├─ SM_CircularBand.fbx
│  │  │  │  │  │  ├─ SM_CircularBand.uasset
│  │  │  │  │  │  ├─ SM_CircularGlow.fbx
│  │  │  │  │  │  └─ SM_CircularGlow.uasset
│  │  │  │  │  └─ NS_JumpPad.uasset
│  │  │  │  └─ BP_JumpPad.uasset
│  │  │  └─ Target/
│  │  │     ├─ Assets/
│  │  │     │  ├─ SM_TargetBaseMesh.fbx
│  │  │     │  └─ SM_TargetBaseMesh.uasset
│  │  │     └─ BP_WobbleTarget.uasset
│  │  ├─ Materials/
│  │  │  ├─ M_FlatCol.uasset
│  │  │  ├─ M_PrototypeGrid.uasset
│  │  │  ├─ MF_ProcGrid.uasset
│  │  │  ├─ MI_DefaultColorway.uasset
│  │  │  ├─ MI_PrototypeGrid_Gray_02.uasset
│  │  │  ├─ MI_PrototypeGrid_Gray.uasset
│  │  │  └─ MI_PrototypeGrid_TopDark.uasset
│  │  ├─ Meshes/
│  │  │  ├─ SM_ChamferCube.fbx
│  │  │  ├─ SM_ChamferCube.uasset
│  │  │  ├─ SM_Cube.uasset
│  │  │  ├─ SM_Cylinder.fbx
│  │  │  ├─ SM_Cylinder.uasset
│  │  │  ├─ SM_QuarterCylinder.fbx
│  │  │  ├─ SM_QuarterCylinder.uasset
│  │  │  ├─ SM_Ramp.uasset
│  │  │  └─ SM_SM_ChamferCube.fbx
│  │  └─ Textures/
│  │     └─ T_GridChecker_A.uasset
│  ├─ ProjectArcaneArena/
│  │  ├─ Characters/
│  │  │  ├─ Boss/
│  │  │  │  ├─ AI/
│  │  │  │  │  ├─ BB_ArenaBoss.uasset
│  │  │  │  │  ├─ BP_ArenaBossAIController.uasset
│  │  │  │  │  └─ BT_ArenaBoss.uasset
│  │  │  │  ├─ Animation/
│  │  │  │  │  ├─ ABP_ArenaBoss.uasset
│  │  │  │  │  ├─ AM_BossCharge.uasset
│  │  │  │  │  ├─ AM_BossDeath.uasset
│  │  │  │  │  ├─ AM_BossFireZone.uasset
│  │  │  │  │  ├─ AM_BossGroundSlam.uasset
│  │  │  │  │  ├─ AM_BossSummonMinions.uasset
│  │  │  │  │  ├─ AS_BossCharge.uasset
│  │  │  │  │  ├─ AS_BossDeath.uasset
│  │  │  │  │  ├─ AS_BossFireZone.uasset
│  │  │  │  │  ├─ AS_BossGroundSlam.uasset
│  │  │  │  │  └─ AS_BossSummonMinions.uasset
│  │  │  │  ├─ Character/
│  │  │  │  │  ├─ BP_ArenaBossCharacter.uasset
│  │  │  │  │  └─ SKM_ArenaBoss.uasset
│  │  │  │  ├─ GAS/
│  │  │  │  │  ├─ Area/
│  │  │  │  │  │  └─ BP_ArenaBossFireZoneArea.uasset
│  │  │  │  │  ├─ GameplayAbility/
│  │  │  │  │  │  ├─ GA_BossCharge.uasset
│  │  │  │  │  │  ├─ GA_BossFireZone.uasset
│  │  │  │  │  │  ├─ GA_BossGroundSlam.uasset
│  │  │  │  │  │  └─ GA_BossSummonMinions.uasset
│  │  │  │  │  ├─ GameplayCue/
│  │  │  │  │  │  ├─ GCN_BossCharge_Active.uasset
│  │  │  │  │  │  ├─ GCN_BossCharge_Impact.uasset
│  │  │  │  │  │  ├─ GCN_BossCharge_Telegraph.uasset
│  │  │  │  │  │  ├─ GCN_BossDeath.uasset
│  │  │  │  │  │  ├─ GCN_BossEnraged_Active.uasset
│  │  │  │  │  │  ├─ GCN_BossFireZone_Active.uasset
│  │  │  │  │  │  ├─ GCN_BossFireZone_Telegraph.uasset
│  │  │  │  │  │  ├─ GCN_BossGroundSlam_Impact.uasset
│  │  │  │  │  │  ├─ GCN_BossGroundSlam_Telegraph.uasset
│  │  │  │  │  │  ├─ GCN_BossPhase_Transition.uasset
│  │  │  │  │  │  ├─ GCN_BossSummon_Cast.uasset
│  │  │  │  │  │  └─ GCN_BossSummon_Spawn.uasset
│  │  │  │  │  └─ GameplayEffect/
│  │  │  │  │     ├─ GE_Boss_Enrage.uasset
│  │  │  │  │     ├─ GE_Boss_PlayerCountScaling.uasset
│  │  │  │  │     ├─ GE_Cooldown_BossCharge.uasset
│  │  │  │  │     ├─ GE_Cooldown_BossFireZone.uasset
│  │  │  │  │     ├─ GE_Cooldown_BossGroundSlam.uasset
│  │  │  │  │     ├─ GE_Cooldown_BossSummonMinions.uasset
│  │  │  │  │     └─ GE_Init_BossAttributes.uasset
│  │  │  │  └─ VFX/
│  │  │  │     ├─ NS_BossCharge_Active.uasset
│  │  │  │     ├─ NS_BossCharge_Impact.uasset
│  │  │  │     ├─ NS_BossCharge_Telegraph.uasset
│  │  │  │     ├─ NS_BossDeath.uasset
│  │  │  │     ├─ NS_BossEnraged_Active.uasset
│  │  │  │     ├─ NS_BossFireZone_Active.uasset
│  │  │  │     ├─ NS_BossFireZone_Telegraph.uasset
│  │  │  │     ├─ NS_BossGroundSlam_Impact.uasset
│  │  │  │     ├─ NS_BossGroundSlam_Telegraph.uasset
│  │  │  │     ├─ NS_BossPhase_Transition.uasset
│  │  │  │     ├─ NS_BossSummon_Cast.uasset
│  │  │  │     └─ NS_BossSummon_Spawn.uasset
│  │  │  ├─ Enemies/
│  │  │  │  ├─ Data/
│  │  │  │  │  └─ Affixes/
│  │  │  │  │     ├─ DA_EnemyAffix_ArcaneWarden.uasset
│  │  │  │  │     ├─ DA_EnemyAffix_Frenzy.uasset
│  │  │  │  │     └─ DA_EnemyAffix_Volatile.uasset
│  │  │  │  ├─ ABP_ArenaEnemy.uasset
│  │  │  │  ├─ BP_ArenaEnemyCharacter.uasset
│  │  │  │  └─ BP_ArenaRangedEnemy.uasset
│  │  │  └─ Player/
│  │  │     └─ BP_ArenaPlayerCharacter.uasset
│  │  ├─ Combat/
│  │  │  ├─ Areas/
│  │  │  │  ├─ Dash/
│  │  │  │  │  └─ BP_ArenaDashTrailArea.uasset
│  │  │  │  └─ LightningStorm/
│  │  │  │     └─ BP_ArenaLightningStormArea.uasset
│  │  │  ├─ Feedback/
│  │  │  │  ├─ Audio/
│  │  │  │  │  └─ SA_ArenaHitFeedback.uasset
│  │  │  │  ├─ Camera/
│  │  │  │  │  ├─ CS_DamageHeavy.uasset
│  │  │  │  │  ├─ CS_DamageLight.uasset
│  │  │  │  │  ├─ CS_DamageMedium.uasset
│  │  │  │  │  └─ CS_ShieldBreak.uasset
│  │  │  │  └─ Materials/
│  │  │  │     └─ M_ArenaHitFlashOverlay.uasset
│  │  │  ├─ GAS/
│  │  │  │  ├─ Abilities/
│  │  │  │  │  ├─ Enemy/
│  │  │  │  │  │  ├─ GA_EnemyMeleeAttack.uasset
│  │  │  │  │  │  └─ GA_EnemyRangedAttack.uasset
│  │  │  │  │  ├─ Player/
│  │  │  │  │  │  ├─ GA_BasicAttack.uasset
│  │  │  │  │  │  ├─ GA_Dash.uasset
│  │  │  │  │  │  ├─ GA_DashLightningTrail.uasset
│  │  │  │  │  │  ├─ GA_Fireball.uasset
│  │  │  │  │  │  ├─ GA_LightningStorm.uasset
│  │  │  │  │  │  ├─ GA_Overload.uasset
│  │  │  │  │  │  ├─ GA_Shield.uasset
│  │  │  │  │  │  └─ GA_ShieldBreakBlast.uasset
│  │  │  │  │  └─ Triggers/
│  │  │  │  │     ├─ GA_EnergyOnAbilityCast.uasset
│  │  │  │  │     ├─ GA_EnergyOnCrit.uasset
│  │  │  │  │     └─ GA_EnergyOnKill.uasset
│  │  │  │  ├─ Cues/
│  │  │  │  │  ├─ Elite/
│  │  │  │  │  │  ├─ GCN_Elite_ArcaneWarden_Active.uasset
│  │  │  │  │  │  ├─ GCN_Elite_ArcaneWarden_Pulse.uasset
│  │  │  │  │  │  ├─ GCN_Elite_Frenzy_Active.uasset
│  │  │  │  │  │  ├─ GCN_Elite_Frenzy_Trigger.uasset
│  │  │  │  │  │  ├─ GCN_Elite_Volatile_Active.uasset
│  │  │  │  │  │  ├─ GCN_Elite_Volatile_Explode.uasset
│  │  │  │  │  │  └─ GCN_Elite_Volatile_Telegraph.uasset
│  │  │  │  │  ├─ Instant/
│  │  │  │  │  │  ├─ GCN_BasicAttack_Activate.uasset
│  │  │  │  │  │  ├─ GCN_DamageCritical.uasset
│  │  │  │  │  │  ├─ GCN_DamageNumber.uasset
│  │  │  │  │  │  ├─ GCN_EnemyMelee_Activate.uasset
│  │  │  │  │  │  ├─ GCN_Fireball_Cast.uasset
│  │  │  │  │  │  ├─ GCN_HealthHit.uasset
│  │  │  │  │  │  ├─ GCN_Hit_Fire.uasset
│  │  │  │  │  │  ├─ GCN_Hit_Lightning.uasset
│  │  │  │  │  │  ├─ GCN_Hit_Physical.uasset
│  │  │  │  │  │  ├─ GCN_LightningStorm_Cast.uasset
│  │  │  │  │  │  ├─ GCN_Overload_Explosion.uasset
│  │  │  │  │  │  ├─ GCN_ShieldBreak_Burst.uasset
│  │  │  │  │  │  ├─ GCN_ShieldBreak.uasset
│  │  │  │  │  │  ├─ GCN_ShieldBreakHealthHit.uasset
│  │  │  │  │  │  └─ GCN_ShieldHit.uasset
│  │  │  │  │  └─ Looping/
│  │  │  │  │     ├─ GCN_Burning_Active.uasset
│  │  │  │  │     ├─ GCN_Dash_Active.uasset
│  │  │  │  │     ├─ GCN_DashLightningTrail_Active.uasset
│  │  │  │  │     ├─ GCN_LightningStorm_Active.uasset
│  │  │  │  │     ├─ GCN_Shield_Active.uasset
│  │  │  │  │     └─ GCN_Shocked_Active.uasset
│  │  │  │  └─ Effects/
│  │  │  │     ├─ Cooldowns/
│  │  │  │     │  ├─ GE_Cooldown_BasicAttack.uasset
│  │  │  │     │  ├─ GE_Cooldown_Dash.uasset
│  │  │  │     │  ├─ GE_Cooldown_EnemyMeleeAttack.uasset
│  │  │  │     │  ├─ GE_Cooldown_EnemyRangedAttack.uasset
│  │  │  │     │  ├─ GE_Cooldown_Fireball.uasset
│  │  │  │     │  ├─ GE_Cooldown_LightningStorm.uasset
│  │  │  │     │  └─ GE_Cooldown_Shield.uasset
│  │  │  │     ├─ Core/
│  │  │  │     │  ├─ GE_Damage.uasset
│  │  │  │     │  ├─ GE_Shield_Grant.uasset
│  │  │  │     │  └─ GE_Shield.uasset
│  │  │  │     ├─ Costs/
│  │  │  │     │  ├─ GE_Cost_Fireball.uasset
│  │  │  │     │  ├─ GE_Cost_LightningStorm.uasset
│  │  │  │     │  └─ GE_Cost_Shield.uasset
│  │  │  │     ├─ Enemy/
│  │  │  │     │  └─ Elite/
│  │  │  │     │     ├─ GE_Elite_BaseAttributes.uasset
│  │  │  │     │     └─ GE_Elite_Frenzy.uasset
│  │  │  │     ├─ Init/
│  │  │  │     │  ├─ GE_Init_EnemyAttributes.uasset
│  │  │  │     │  └─ GE_Init_PlayerAttributes.uasset
│  │  │  │     ├─ Status/
│  │  │  │     │  ├─ GE_Status_Burning.uasset
│  │  │  │     │  ├─ GE_Status_OverloadLockout.uasset
│  │  │  │     │  ├─ GE_Status_Shocked.uasset
│  │  │  │     │  └─ GE_Status_Stunned.uasset
│  │  │  │     ├─ Triggers/
│  │  │  │     │  ├─ GE_Trigger_EnergyOnAbilityCast.uasset
│  │  │  │     │  ├─ GE_Trigger_EnergyOnCrit.uasset
│  │  │  │     │  └─ GE_Trigger_EnergyOnKill.uasset
│  │  │  │     └─ Upgrades/
│  │  │  │        ├─ GE_Upgrade_AttackPower.uasset
│  │  │  │        ├─ GE_Upgrade_CritChance.uasset
│  │  │  │        ├─ GE_Upgrade_MaxHealth.uasset
│  │  │  │        └─ GE_Upgrade_MoveSpeed.uasset
│  │  │  ├─ Projectiles/
│  │  │  │  └─ Actors/
│  │  │  │     ├─ BP_ArenaEnemyProjectile.uasset
│  │  │  │     └─ BP_ArenaFireballProjectile.uasset
│  │  │  └─ Weapons/
│  │  │     └─ Data/
│  │  │        ├─ DA_Weapon_ArcaneBolt_Fast.uasset
│  │  │        ├─ DA_Weapon_ArcaneBolt.uasset
│  │  │        └─ DA_Weapon_Shotgun.uasset
│  │  ├─ Core/
│  │  │  ├─ Controllers/
│  │  │  │  └─ BP_ArenaPlayerController.uasset
│  │  │  └─ GameMode/
│  │  │     └─ BP_ArenaGameMode.uasset
│  │  ├─ Dev/
│  │  │  ├─ Experiments/
│  │  │  │  └─ Mass/
│  │  │  │     └─ BP_MassCluster.uasset
│  │  │  ├─ LegacyTemplate/
│  │  │  │  └─ TopDown/
│  │  │  │     ├─ Blueprints/
│  │  │  │     │  ├─ BP_TopDownCharacter.uasset
│  │  │  │     │  ├─ BP_TopDownController.uasset
│  │  │  │     │  └─ BP_TopDownGameMode.uasset
│  │  │  │     └─ Cursor/
│  │  │  │        ├─ FX_Cursor_Failure.uasset
│  │  │  │        ├─ FX_Cursor_Success.uasset
│  │  │  │        ├─ M_Cursor.uasset
│  │  │  │        ├─ MI_Cursor_Red.uasset
│  │  │  │        ├─ SM_CursorMesh_Red.uasset
│  │  │  │        ├─ SM_CursorMesh.uasset
│  │  │  │        └─ T_Arrow.uasset
│  │  │  └─ Tests/
│  │  │     ├─ Overload/
│  │  │     │  ├─ BP_ArenaEnemy_OverloadDummy.uasset
│  │  │     │  ├─ BP_ArenaGameMode_OverloadTest.uasset
│  │  │     │  ├─ BP_ArenaUpgradeTestPickup.uasset
│  │  │     │  ├─ GE_Test_OverloadDummyAttributes.uasset
│  │  │     │  └─ Lvl_OverloadTest.umap
│  │  │     ├─ PCG/
│  │  │     │  ├─ forest/
│  │  │     │  │  └─ PCG_Forest.uasset
│  │  │     │  ├─ Lvl_ForestLab_sharedassets/
│  │  │     │  │  ├─ LI_L1.uasset
│  │  │     │  │  ├─ LI_L2.uasset
│  │  │     │  │  ├─ LI_L3.uasset
│  │  │     │  │  ├─ LI_L4.uasset
│  │  │     │  │  ├─ LI_L5.uasset
│  │  │     │  │  └─ LI_L6.uasset
│  │  │     │  ├─ Lvl_ForestLab.umap
│  │  │     │  ├─ Lvl_PCGLab.umap
│  │  │     │  ├─ PCG_Decor_Lab_Minimal.uasset
│  │  │     │  └─ PCG_Decor_Lab.uasset
│  │  │     └─ Projectile/
│  │  │        └─ BP_ArenaProjectileStressTestActor.uasset
│  │  ├─ Input/
│  │  │  ├─ Actions/
│  │  │  │  ├─ IA_SetDestination_Click.uasset
│  │  │  │  └─ IA_SetDestination_Touch.uasset
│  │  │  └─ Contexts/
│  │  │     └─ IMC_Default.uasset
│  │  ├─ Systems/
│  │  │  ├─ Inventory/
│  │  │  │  ├─ Data/
│  │  │  │  │  ├─ DA_EnergyPotion.uasset
│  │  │  │  │  └─ DA_HealthPotion.uasset
│  │  │  │  ├─ Effects/
│  │  │  │  │  └─ GE_Cooldown_ItemConsumable.uasset
│  │  │  │  └─ Pickups/
│  │  │  │     ├─ BP_EnergyPotionPickup.uasset
│  │  │  │     └─ BP_HealthPotionPickup.uasset
│  │  │  ├─ Pickups/
│  │  │  │  ├─ Art/
│  │  │  │  │  ├─ HealthCrystal/
│  │  │  │  │  │  ├─ Materials/
│  │  │  │  │  │  │  ├─ HealthCrystal_BaseColor.uasset
│  │  │  │  │  │  │  ├─ HealthCrystal_Emissive.uasset
│  │  │  │  │  │  │  ├─ HealthCrystal_Normal.uasset
│  │  │  │  │  │  │  ├─ HealthCrystal_OcclusionRoughnessMetallic.uasset
│  │  │  │  │  │  │  ├─ M_HealthCrystal.uasset
│  │  │  │  │  │  │  └─ MI_HealthCrystal.uasset
│  │  │  │  │  │  └─ SM_HealthCrystal.uasset
│  │  │  │  │  └─ Potions/
│  │  │  │  │     ├─ Bottle/
│  │  │  │  │     │  ├─ M_Bottle.uasset
│  │  │  │  │     │  ├─ MI_Bottle.uasset
│  │  │  │  │     │  ├─ Potion_low_Bottle_BaseColor.uasset
│  │  │  │  │     │  ├─ Potion_low_Bottle_Emissive.uasset
│  │  │  │  │     │  ├─ Potion_low_Bottle_Normal.uasset
│  │  │  │  │     │  ├─ Potion_low_Bottle_OcclusionRoughnessMetallic.uasset
│  │  │  │  │     │  └─ Potion_low_Bottle_Opacity.uasset
│  │  │  │  │     ├─ Cork/
│  │  │  │  │     │  ├─ M_Cork.uasset
│  │  │  │  │     │  ├─ T_Cork_BaseColor.uasset
│  │  │  │  │     │  ├─ T_Cork_Normal.uasset
│  │  │  │  │     │  └─ T_Cork_OcclusionRoughnessMetallic.uasset
│  │  │  │  │     ├─ Liquid/
│  │  │  │  │     │  ├─ M_WhiteLiquid.uasset
│  │  │  │  │     │  ├─ MI_BlueLiquid.uasset
│  │  │  │  │     │  ├─ MI_RedLiquid.uasset
│  │  │  │  │     │  ├─ T_WhiteLiquid_BaseColor.uasset
│  │  │  │  │     │  ├─ T_WhiteLiquid_Emissive.uasset
│  │  │  │  │     │  ├─ T_WhiteLiquid_Normal.uasset
│  │  │  │  │     │  ├─ T_WhiteLiquid_OcclusionRoughnessMetallic.uasset
│  │  │  │  │     │  └─ T_WhiteLiquid_Opacity.uasset
│  │  │  │  │     └─ SM_PotionBottle.uasset
│  │  │  │  ├─ Blueprints/
│  │  │  │  │  ├─ BP_EnergyPickup.uasset
│  │  │  │  │  └─ BP_HealthPickup.uasset
│  │  │  │  └─ Data/
│  │  │  │     └─ DA_PickupDropTable_Default.uasset
│  │  │  ├─ Upgrades/
│  │  │  │  └─ Data/
│  │  │  │     ├─ DA_Upgrade_AttackPower.uasset
│  │  │  │     ├─ DA_Upgrade_CritChance.uasset
│  │  │  │     ├─ DA_Upgrade_DashCooldown.uasset
│  │  │  │     ├─ DA_Upgrade_DashLightningTrail.uasset
│  │  │  │     ├─ DA_Upgrade_EnergyOnAbilityCast.uasset
│  │  │  │     ├─ DA_Upgrade_EnergyOnCrit.uasset
│  │  │  │     ├─ DA_Upgrade_EnergyOnKill.uasset
│  │  │  │     ├─ DA_Upgrade_FireballBurning.uasset
│  │  │  │     ├─ DA_Upgrade_FireballDamage.uasset
│  │  │  │     ├─ DA_Upgrade_LightningStormDamage.uasset
│  │  │  │     ├─ DA_Upgrade_LightningStormShocked.uasset
│  │  │  │     ├─ DA_Upgrade_MaxHealth.uasset
│  │  │  │     ├─ DA_Upgrade_MoveSpeed.uasset
│  │  │  │     ├─ DA_Upgrade_Overload.uasset
│  │  │  │     ├─ DA_Upgrade_ShieldAmount.uasset
│  │  │  │     └─ DA_Upgrade_ShieldBreakBlast.uasset
│  │  │  └─ Waves/
│  │  │     └─ Data/
│  │  │        ├─ DA_Waves_Prototype_test.uasset
│  │  │        └─ DA_Waves_Prototype.uasset
│  │  ├─ UI/
│  │  │  ├─ Cursor/
│  │  │  │  └─ Current/
│  │  │  │     ├─ FX_Cursor.uasset
│  │  │  │     ├─ M_Cursor.uasset
│  │  │  │     ├─ SM_CursorMesh.uasset
│  │  │  │     └─ T_Arrow.uasset
│  │  │  ├─ DamageNumbers/
│  │  │  │  ├─ BP_ArenaDamageNumberActor.uasset
│  │  │  │  └─ WBP_DamageNumber.uasset
│  │  │  ├─ HUD/
│  │  │  │  ├─ Enemy/
│  │  │  │  │  └─ WBP_EnemyHealthBar.uasset
│  │  │  │  └─ WBP_PlayerHUD.uasset
│  │  │  ├─ Inventory/
│  │  │  │  ├─ WBP_Inventory.uasset
│  │  │  │  └─ WBP_InventorySlot.uasset
│  │  │  ├─ MainMenu/
│  │  │  │  ├─ BP_ArenaMainMenuGameMode.uasset
│  │  │  │  ├─ BP_ArenaMainMenuPlayerController.uasset
│  │  │  │  ├─ Lvl_MainMenu.umap
│  │  │  │  └─ WBP_MainMenu.uasset
│  │  │  └─ Upgrade/
│  │  │     └─ Icons/
│  │  │        ├─ T_Upgrade_AttackPower_Icon.png
│  │  │        ├─ T_Upgrade_AttackPower_Icon.uasset
│  │  │        ├─ T_Upgrade_CritChance_Icon.uasset
│  │  │        ├─ T_Upgrade_DashCooldown_Icon.uasset
│  │  │        ├─ T_Upgrade_DashLightningTrail_Icon.uasset
│  │  │        ├─ T_Upgrade_EnergyOnAbilityCast_Icon.uasset
│  │  │        ├─ T_Upgrade_EnergyOnCrit_Icon.uasset
│  │  │        ├─ T_Upgrade_EnergyOnKill_Icon.uasset
│  │  │        ├─ T_Upgrade_FireballBurning_Icon.png
│  │  │        ├─ T_Upgrade_FireballBurning_Icon.uasset
│  │  │        ├─ T_Upgrade_FireballDamage_Icon.png
│  │  │        ├─ T_Upgrade_FireballDamage_Icon.uasset
│  │  │        ├─ T_Upgrade_LightningStormDamage_Icon.png
│  │  │        ├─ T_Upgrade_LightningStormDamage_Icon.uasset
│  │  │        ├─ T_Upgrade_LightningStormShocked_Icon.png
│  │  │        ├─ T_Upgrade_LightningStormShocked_Icon.uasset
│  │  │        ├─ T_Upgrade_MaxHealth_Icon.png
│  │  │        ├─ T_Upgrade_MaxHealth_Icon.uasset
│  │  │        ├─ T_Upgrade_MoveSpeed_Icon.png
│  │  │        ├─ T_Upgrade_MoveSpeed_Icon.uasset
│  │  │        ├─ T_Upgrade_Overload_Icon.uasset
│  │  │        ├─ T_Upgrade_ShieldAmount_Icon.uasset
│  │  │        └─ T_Upgrade_ShieldBreakBlast_Icon.uasset
│  │  ├─ VFX/
│  │  │  └─ Common/
│  │  │     ├─ NS_DashAura.uasset
│  │  │     ├─ NS_LightningStorm.uasset
│  │  │     ├─ NS_Shield_Muriel_BeforeLifetimeFix.uasset
│  │  │     ├─ NS_Shield_Muriel.uasset
│  │  │     └─ NS_ShieldAura.uasset
│  │  └─ World/
│  │     ├─ Maps/
│  │     │  └─ Lvl_Arena.umap
│  │     └─ Materials/
│  │        └─ MI_Colorway.uasset
│  └─ Stylized_Spruce_Forest/
│     ├─ Audio/
│     │  ├─ SC_Day_Forest_Ambient_01.uasset
│     │  └─ SW_Day_Forest_Ambient_01.uasset
│     ├─ Blueprints/
│     │  ├─ Camera_Shake/
│     │  │  ├─ BP_MoveTurnLeftCameraShake.uasset
│     │  │  ├─ BP_MoveTurnRightCameraShake.uasset
│     │  │  ├─ BP_SprintCameraShake.uasset
│     │  │  ├─ BP_TurnLeftCameraShake.uasset
│     │  │  ├─ BP_TurnRightCameraShake.uasset
│     │  │  └─ BP_WalkCameraShake.uasset
│     │  ├─ BP_Interactive_Foliage.uasset
│     │  └─ BP_Procedural_Seasons.uasset
│     ├─ Demo/
│     │  ├─ Game_Mode/
│     │  │  ├─ FirstPersonBP/
│     │  │  │  └─ Blueprints/
│     │  │  │     ├─ STZD_FirstPersonCharacter.uasset
│     │  │  │     ├─ STZD_FirstPersonGameMode.uasset
│     │  │  │     └─ STZD_FirstPersonHUD.uasset
│     │  │  ├─ Mannequin/
│     │  │  │  ├─ Animations/
│     │  │  │  │  ├─ ThirdPerson_AnimBP.uasset
│     │  │  │  │  ├─ ThirdPerson_IdleRun_2D.uasset
│     │  │  │  │  ├─ ThirdPerson_Jump.uasset
│     │  │  │  │  ├─ ThirdPersonIdle.uasset
│     │  │  │  │  ├─ ThirdPersonJump_End.uasset
│     │  │  │  │  ├─ ThirdPersonJump_Loop.uasset
│     │  │  │  │  ├─ ThirdPersonJump_Start.uasset
│     │  │  │  │  ├─ ThirdPersonRun.uasset
│     │  │  │  │  └─ ThirdPersonWalk.uasset
│     │  │  │  └─ Character/
│     │  │  │     ├─ Materials/
│     │  │  │     │  ├─ MaterialLayers/
│     │  │  │     │  │  ├─ ML_GlossyBlack_Latex_UE4.uasset
│     │  │  │     │  │  ├─ ML_Plastic_Shiny_Beige_LOGO.uasset
│     │  │  │     │  │  ├─ ML_Plastic_Shiny_Beige.uasset
│     │  │  │     │  │  ├─ ML_SoftMetal_UE4.uasset
│     │  │  │     │  │  ├─ T_ML_Aluminum01_N.uasset
│     │  │  │     │  │  ├─ T_ML_Aluminum01.uasset
│     │  │  │     │  │  ├─ T_ML_Rubber_Blue_01_D.uasset
│     │  │  │     │  │  └─ T_ML_Rubber_Blue_01_N.uasset
│     │  │  │     │  ├─ M_Male_Body.uasset
│     │  │  │     │  ├─ M_UE4Man_ChestLogo.uasset
│     │  │  │     │  └─ MI_Female_Body.uasset
│     │  │  │     ├─ Mesh/
│     │  │  │     │  ├─ SK_Mannequin_Female_PhysicsAsset.uasset
│     │  │  │     │  ├─ SK_Mannequin_Female.uasset
│     │  │  │     │  ├─ SK_Mannequin_PhysicsAsset.uasset
│     │  │  │     │  ├─ SK_Mannequin.uasset
│     │  │  │     │  └─ UE4_Mannequin_Skeleton.uasset
│     │  │  │     └─ Textures/
│     │  │  │        ├─ T_Female_Mask.uasset
│     │  │  │        ├─ T_Female_N.uasset
│     │  │  │        ├─ T_Male_Mask.uasset
│     │  │  │        ├─ T_Male_N.uasset
│     │  │  │        ├─ T_UE4Logo_Mask.uasset
│     │  │  │        └─ T_UE4Logo_N.uasset
│     │  │  └─ ThirdPersonBP/
│     │  │     └─ Blueprints/
│     │  │        ├─ STZD_ThirdPersonCharacter.uasset
│     │  │        └─ STZD_ThirdPersonGameMode.uasset
│     │  └─ Maps/
│     │     ├─ STZD_Demo_01.umap
│     │     └─ STZD_Overview.umap
│     ├─ Landscape_Layers/
│     │  ├─ L1_LayerInfo.uasset
│     │  ├─ L2_LayerInfo.uasset
│     │  ├─ L3_LayerInfo.uasset
│     │  ├─ L4_LayerInfo.uasset
│     │  ├─ L5_LayerInfo.uasset
│     │  ├─ L6_LayerInfo.uasset
│     │  └─ Remove_Procedural_LayerInfo.uasset
│     ├─ Materials/
│     │  ├─ Master_Materials/
│     │  │  ├─ M_Foliage.uasset
│     │  │  ├─ M_Landscape.uasset
│     │  │  ├─ M_Oil_Lantern_Glass.uasset
│     │  │  ├─ M_Oil_Lantern_Main.uasset
│     │  │  └─ M_Water.uasset
│     │  ├─ Material_Functions/
│     │  │  ├─ MF_Detail_Normal.uasset
│     │  │  ├─ MF_Foliage_Interactive.uasset
│     │  │  ├─ MF_Foliage_Winter_SSS_Color.uasset
│     │  │  ├─ MF_Landscape_Layer_Base.uasset
│     │  │  ├─ MF_Opacity.uasset
│     │  │  ├─ MF_Procedural_Seasons.uasset
│     │  │  ├─ MF_Puddles.uasset
│     │  │  ├─ MF_Slope_Mask.uasset
│     │  │  ├─ MF_Wind_Ripples.uasset
│     │  │  └─ MF_Wind.uasset
│     │  ├─ Material_Instances/
│     │  │  ├─ Background/
│     │  │  │  ├─ MI_Background_01.uasset
│     │  │  │  ├─ MI_Background_02.uasset
│     │  │  │  └─ MI_Background_03.uasset
│     │  │  ├─ Particles/
│     │  │  │  ├─ MI_Leaf_Autumn.uasset
│     │  │  │  ├─ MI_Rain.uasset
│     │  │  │  └─ MI_Snow.uasset
│     │  │  ├─ Rocks/
│     │  │  │  ├─ MI_Rocks_01-03.uasset
│     │  │  │  ├─ MI_Rocks_04-05.uasset
│     │  │  │  └─ MI_Rocks_06-09.uasset
│     │  │  ├─ MI_Debris.uasset
│     │  │  ├─ MI_Lake.uasset
│     │  │  ├─ MI_Landscape.uasset
│     │  │  ├─ MI_Meshes.uasset
│     │  │  ├─ MI_Plants_01.uasset
│     │  │  ├─ MI_River.uasset
│     │  │  ├─ MI_Road_01.uasset
│     │  │  └─ MI_Trees.uasset
│     │  └─ Particles/
│     │     ├─ M_Leaf.uasset
│     │     └─ M_Rain.uasset
│     ├─ Meshes/
│     │  ├─ Background/
│     │  │  ├─ SM_Background_Landscape_01.uasset
│     │  │  ├─ SM_Background_Landscape_02.uasset
│     │  │  └─ SM_Background_Landscape_03.uasset
│     │  ├─ Debris/
│     │  │  ├─ SM_Branch_01.uasset
│     │  │  ├─ SM_Broken_Tree_01.uasset
│     │  │  ├─ SM_Dry_Tree_01.uasset
│     │  │  ├─ SM_Dry_Tree_02.uasset
│     │  │  ├─ SM_Log_01.uasset
│     │  │  ├─ SM_Logs_01.uasset
│     │  │  ├─ SM_Logs_02.uasset
│     │  │  ├─ SM_Mushroom_01.uasset
│     │  │  ├─ SM_Mushroom_02.uasset
│     │  │  ├─ SM_Mushroom_03.uasset
│     │  │  └─ SM_Stump_01.uasset
│     │  ├─ Plants/
│     │  │  ├─ SM_Blue_Plant_01.uasset
│     │  │  ├─ SM_Blue_Plant_02.uasset
│     │  │  ├─ SM_Bush_01.uasset
│     │  │  ├─ SM_Dry_Bush_01.uasset
│     │  │  ├─ SM_Dry_Bush_02.uasset
│     │  │  ├─ SM_Flowers_01.uasset
│     │  │  ├─ SM_Grass_01.uasset
│     │  │  ├─ SM_Grass_02.uasset
│     │  │  ├─ SM_Plant_01.uasset
│     │  │  ├─ SM_Red_Plant_01.uasset
│     │  │  ├─ SM_Spruce_Bush_01.uasset
│     │  │  └─ SM_White_Plant_01.uasset
│     │  ├─ Rocks/
│     │  │  ├─ SM_Rock_01.uasset
│     │  │  ├─ SM_Rock_02.uasset
│     │  │  ├─ SM_Rock_03.uasset
│     │  │  ├─ SM_Rock_04.uasset
│     │  │  ├─ SM_Rock_05.uasset
│     │  │  ├─ SM_Rock_06.uasset
│     │  │  ├─ SM_Rock_07.uasset
│     │  │  ├─ SM_Rock_08.uasset
│     │  │  ├─ SM_Rock_09.uasset
│     │  │  ├─ SM_Small_Rock_01.uasset
│     │  │  ├─ SM_Small_Rock_02.uasset
│     │  │  └─ SM_Small_Rock_03.uasset
│     │  ├─ Trees/
│     │  │  ├─ SM_Small_Spruce_01.uasset
│     │  │  ├─ SM_Spruce_01.uasset
│     │  │  ├─ SM_Spruce_02.uasset
│     │  │  ├─ SM_Spruce_03.uasset
│     │  │  ├─ SM_Spruce_04.uasset
│     │  │  ├─ SM_Spruce_05.uasset
│     │  │  ├─ SM_Spruce_06.uasset
│     │  │  └─ SM_Spruce_07.uasset
│     │  ├─ SM_Lake.uasset
│     │  ├─ SM_Oil_Lantern_01.uasset
│     │  ├─ SM_River.uasset
│     │  └─ SM_Road_01.uasset
│     ├─ MPC/
│     │  └─ MPC_Global.uasset
│     ├─ Particles/
│     │  ├─ NS_Autumn.uasset
│     │  ├─ NS_Rain.uasset
│     │  └─ NS_Snow.uasset
│     ├─ Procedural/
│     │  ├─ Landscape_Grass_Types/
│     │  │  ├─ LGT_Bushes.uasset
│     │  │  ├─ LGT_Flowers.uasset
│     │  │  ├─ LGT_Grass.uasset
│     │  │  ├─ LGT_Peblees.uasset
│     │  │  └─ LGT_Small_Debris.uasset
│     │  ├─ SMF_Logs/
│     │  │  ├─ SMF_Branch_01.uasset
│     │  │  ├─ SMF_Broken_Tree_01.uasset
│     │  │  ├─ SMF_Dry_Tree_01.uasset
│     │  │  ├─ SMF_Dry_Tree_02.uasset
│     │  │  ├─ SMF_Log_01.uasset
│     │  │  ├─ SMF_Logs_01.uasset
│     │  │  ├─ SMF_Logs_02.uasset
│     │  │  └─ SMF_Stump_01.uasset
│     │  ├─ SMF_Rocks/
│     │  │  ├─ SMF_Rock_01.uasset
│     │  │  ├─ SMF_Rock_02.uasset
│     │  │  ├─ SMF_Rock_03.uasset
│     │  │  ├─ SMF_Rock_04.uasset
│     │  │  ├─ SMF_Rock_05.uasset
│     │  │  ├─ SMF_Slope_Rock_04.uasset
│     │  │  └─ SMF_Slope_Rock_05.uasset
│     │  ├─ SMF_Trees/
│     │  │  ├─ SMF_Small_Spruce_01.uasset
│     │  │  ├─ SMF_Spruce_01.uasset
│     │  │  ├─ SMF_Spruce_02.uasset
│     │  │  ├─ SMF_Spruce_03.uasset
│     │  │  ├─ SMF_Spruce_04.uasset
│     │  │  ├─ SMF_Spruce_05.uasset
│     │  │  ├─ SMF_Spruce_06.uasset
│     │  │  └─ SMF_Spruce_07.uasset
│     │  ├─ PFS_Big_Trees_01.uasset
│     │  ├─ PFS_Debris_01.uasset
│     │  ├─ PFS_Rocks_01.uasset
│     │  ├─ PFS_Slope_Rocks_01.uasset
│     │  └─ PFS_Small_Trees_01.uasset
│     ├─ Textures/
│     │  ├─ Background/
│     │  │  ├─ 01/
│     │  │  │  ├─ T_Background_Landscape_01_Albedo.uasset
│     │  │  │  └─ T_Background_Landscape_01_Normal.uasset
│     │  │  ├─ 02/
│     │  │  │  ├─ T_Background_Landscape_02_Albedo.uasset
│     │  │  │  └─ T_Background_Landscape_02_Normal.uasset
│     │  │  └─ 03/
│     │  │     ├─ T_Background_Landscape_03_Albedo.uasset
│     │  │     └─ T_Background_Landscape_03_Normal.uasset
│     │  ├─ FX/
│     │  │  ├─ T_Detail_Normal.uasset
│     │  │  ├─ T_Particle_Normal.uasset
│     │  │  ├─ T_Particles_Albedo.uasset
│     │  │  ├─ T_Plants_Mask.uasset
│     │  │  └─ T_Puddle_Mask.uasset
│     │  ├─ Landscape/
│     │  │  ├─ Grass_01/
│     │  │  │  ├─ T_Grass_01_Albedo.uasset
│     │  │  │  └─ T_Grass_01_Normal.uasset
│     │  │  ├─ Ice/
│     │  │  │  ├─ T_Ice_Albedo.uasset
│     │  │  │  └─ T_Ice_Normal.uasset
│     │  │  ├─ Mud/
│     │  │  │  ├─ T_Mud_01_Albedo.uasset
│     │  │  │  └─ T_Mud_01_Normal.uasset
│     │  │  ├─ Needles/
│     │  │  │  ├─ T_Needles_01_Albedo.uasset
│     │  │  │  └─ T_Needles_01_Normal.uasset
│     │  │  ├─ Peebles/
│     │  │  │  ├─ T_Peebles_01_Albedo.uasset
│     │  │  │  └─ T_Peebles_01_Normal.uasset
│     │  │  ├─ River_Peebles_01/
│     │  │  │  ├─ T_River_Peebles_01_Albedo.uasset
│     │  │  │  └─ T_River_Peebles_01_Normal.uasset
│     │  │  ├─ Road_01/
│     │  │  │  ├─ T_Road_01_Albedo.uasset
│     │  │  │  └─ T_Road_01_Normal.uasset
│     │  │  ├─ Slope/
│     │  │  │  ├─ T_Slope_01_Albedo.uasset
│     │  │  │  └─ T_Slope_01_Normal.uasset
│     │  │  └─ Snow/
│     │  │     ├─ T_Snow_Albedo.uasset
│     │  │     └─ T_Snow_Normal.uasset
│     │  ├─ Oil_Lantern/
│     │  │  ├─ T_Oil_Lantern_01_Albedo.uasset
│     │  │  ├─ T_Oil_Lantern_01_Normal.uasset
│     │  │  └─ T_Oil_Lantern_Roughness_Metallic.uasset
│     │  ├─ Plants/
│     │  │  ├─ Leaf_01/
│     │  │  │  ├─ T_Leaf_01_Albedo.uasset
│     │  │  │  └─ T_Leaf_01_Normal.uasset
│     │  │  └─ Plants_Assembly_01/
│     │  │     ├─ T_Plants_Assembly_01_Albedo.uasset
│     │  │     └─ T_Plants_Assembly_01_Normal.uasset
│     │  ├─ Rocks/
│     │  │  ├─ Rocks_01-03/
│     │  │  │  ├─ T_Rocks_01-03_Albedo.uasset
│     │  │  │  └─ T_Rocks_01-03_Normal.uasset
│     │  │  ├─ Rocks_04-05/
│     │  │  │  ├─ T_Rocks_04-05_Albedo.uasset
│     │  │  │  └─ T_Rocks_04-05_Normal.uasset
│     │  │  └─ Rocks_06-09/
│     │  │     ├─ T_Rocks_06-09_Albedo.uasset
│     │  │     └─ T_Rocks_06-09_Normal.uasset
│     │  ├─ Trees/
│     │  │  ├─ T_Spruces_Albedo_01.uasset
│     │  │  └─ T_Spruces_Normal_01.uasset
│     │  ├─ Water/
│     │  │  └─ T_Water_Normal.uasset
│     │  └─ T_Interface.uasset
│     └─ UI/
│        └─ WB_Interface.uasset
├─ Docs/
│  └─ Archive/
│     └─ mcp-conversation-export.md
└─ Scripts/
   └─ Python/
      ├─ balance/
      │  └─ apply_resume_demo_balance.py
      ├─ boss/
      │  ├─ README.md
      │  ├─ setup_boss_charge.py
      │  ├─ setup_boss_decision.py
      │  ├─ setup_boss_fire_zone.py
      │  ├─ setup_boss_foundation.py
      │  ├─ setup_boss_phase_system.py
      │  ├─ setup_boss_player_scaling.py
      │  ├─ setup_boss_summon_minions.py
      │  └─ setup_boss_victory_outro.py
      ├─ build_assets/
      │  ├─ __init__.py
      │  ├─ arena_asset_tools.py
      │  ├─ configure_build_asset_links.py
      │  ├─ generate_actor_blueprints.py
      │  ├─ generate_burst_gameplay_cues.py
      │  ├─ generate_damage_number_gameplay_cues.py
      │  ├─ generate_gameplay_ability_blueprints.py
      │  ├─ generate_gameplay_effect_blueprints.py
      │  ├─ generate_looping_gameplay_cues.py
      │  ├─ generate_upgrade_assets.py
      │  ├─ README.md
      │  └─ setup_build_assets.py
      ├─ damage_feedback/
      │  ├─ README.md
      │  └─ setup_damage_feedback_polish.py
      ├─ elite_enemy/
      │  ├─ README.md
      │  └─ setup_elite_enemy.py
      ├─ inventory/
      │  ├─ __init__.py
      │  ├─ README.md
      │  └─ setup_inventory_items.py
      ├─ other/
      │  ├─ fix_muriel_shield_niagara_lifetime.py
      │  ├─ import_upgrade_icons.py
      │  ├─ setup_muriel_shield_cue.py
      │  └─ setup_wukong_dash.py
      ├─ overload_test/
      │  ├─ README.md
      │  └─ setup_overload_test.py
      ├─ pcg_lab/
      │  ├─ deep_diagnose.py
      │  ├─ diagnose_pcg.py
      │  ├─ enable_forest_collision.py
      │  ├─ fix_mesh_selector.py
      │  ├─ generate_pcg_lab.py
      │  ├─ rebuild_pcg_graph.py
      │  ├─ replace_forest_meshes.py
      │  ├─ setup_forest_lab.py
      │  └─ setup_pcg_lab.py
      ├─ pickup_items/
      │  ├─ __init__.py
      │  ├─ README.md
      │  └─ setup_pickup_items.py
      ├─ projectile_stress/
      │  ├─ README.md
      │  └─ setup_projectile_stress.py
      ├─ ranged_enemy/
      │  ├─ README.md
      │  └─ setup_ranged_enemy.py
      ├─ setup_build_assets.py
      ├─ setup_main_menu.py
      └─ setup_pickup_items.py
```

## 4. UE 自动管理文件

以下 162 个文件属于 `__ExternalActors__` / `__ExternalObjects__`。**不要在 Windows Explorer 或 Content Browser 中逐个移动这些 hash 文件。**

处理规则：

- `Lvl_TopDown` 迁移/重命名为 `/Game/ProjectArcaneArena/World/Maps/Lvl_Arena` 时，由 Unreal Editor 负责关联的 External Actor / External Object 包。
- `Lvl_OverloadTest` 迁移到 `/Game/ProjectArcaneArena/Dev/Tests/Overload` 时同理。
- 地图迁移后执行 Save All、重启 Editor、重新打开地图，再验证 External Actor 引用。
- 只有完成验证后才处理 Redirector。

当前自动管理文件清单：

```text
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/0/8Z/H6ZQF348KJK5BRINYIW8QN.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/0/C7/6U3UDM4A0XVKST1SOAGTCG.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/0/DS/XK2510RKNZB6T39GCX1XZW.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/0/EK/6LU3U94665QLZDXUMYM4JL.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/0/EK/IM500RL6AIKCHLCFCM9RQC.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/0/JV/E3TDY64OW6BETBTG7NORJ0.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/0/WV/5ZLCB3WUW5FRN1YZ9BCFBU.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/1/5R/WW6OIM344VFJ6P7EJDJ3IY.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/1/6B/N7MUYT5O0TX3T9IA15I3D4.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/1/CY/OUVS3O6RAKAMK58262W218.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/1/MO/LJIZECXMH50M5ITWRP0NXY.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/1/S3/JLV5IBD63NWSDEW9HFAIQI.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/2/4P/ETAA36PEZEU5FF8BDIHVJU.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/2/62/FBXRV2PDUCOU945NYEGAA8.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/2/76/1FZQURUFYWERG3SCUHIY22.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/2/M2/QVK6V1XNZCNWO5IY7MT3Z8.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/2/V0/K654TGT3PG38XSH4NZKKFL.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/2O/JRU7W4XKPRFH3MW8RG0JHV.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/9V/NLJQ4K8M0QSRJVSKY2S3F9.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/AR/T9USQE2PHZD3T21PJDZWV0.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/H4/PUDLBMGNM2RW01K3DBC5T6.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/HN/USNRRATM8ZL8N5CEYMXSW7.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/J4/27HJLR4JPU1U53NICLJ9KE.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/NN/DF52BURN86I8DBZW2VI42L.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/UJ/9WNHYBWMWII4GFMABEODYE.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/3/UY/93DUC5KRF4MS9DP87SNV4O.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/4/19/TBLK073YDD75TF6ZKDAJKN.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/4/6O/Y4ZH1ZZNG8D3AKZHLA9IVG.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/4/8R/SRSAAG01H6FOGT286QGFNT.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/4/AO/57LXWY75EEXGZYTR4L8Y01.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/4/BK/J4EANRH1P94AFYQ6ZUH2K5.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/4/VL/611N8VKM3A75RVR6DSHCPL.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/4/XH/5XF2E3EMCOCC4FUCC2KQK6.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/5/0C/KR176K1WZ6C4RURLD9ROXX.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/5/89/PF46Z417FJ6XN3E92H04NV.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/5/8D/0MSFFQWF47730HH30P0REO.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/5/LH/XIOU34F86W42NERL0R3LHU.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/5/OC/AYMYU2MHVBKJF92TPCL514.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/5/RO/GYRR4M43NJM46YTIHPFCHR.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/5/TB/U6OFGEFT5FGQQS0QIMLEG6.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/5/TP/K3BA527LFP61T9H4PJ3GYK.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/25/6AE1JN3RA9W2RWBDNLCNBF.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/9Z/RRJ26XDWVFAAAJMD2LIK9J.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/B7/FHMJS25AL5PMGKJLJWNJAX.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/C7/923ME62ZN17ABEA7WYX1FF.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/K5/HNQDMW6UQZ2SYZOSVP34OW.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/O0/ETM74LR4BWLSUNS1Z3TXDJ.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/P2/UXEOWPXHE9DR9F5EYIMXPM.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/YT/S6NR5P38FB2KCCM9CHGXO4.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/6/YZ/70M9BKR1T9ZH3O9VHHMKDC.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/7/E3/IBAFOCXDSRYRWJ9HLANY3C.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/7/KD/A6V8ZG15ALWXASN1F232XG.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/7/NC/LK2R14LB94DE2EOY4LBH9O.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/8/GQ/QU3JR04TVNWHWGPFWG9CA0.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/8/IS/0195SD4HDVHT1RBL827QAQ.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/8/OD/CRI3RU9K3J7T7AGAHU9QVE.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/8/V1/0YERKF0OAQERVZ58LRWTFF.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/8/Z4/8Q3816K2SQTMDM032C9ADR.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/9/5U/B4SUF2HRABPSMFLHS9U7AP.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/9/AB/0SRXH9RY80WUYIE7M36NFL.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/9/GD/SCOHWF9V440DFXTMSX0PLI.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/9/HD/XERZAOAVIBJ6IB5MK3OEBA.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/9/R2/35D4MNX7XSI289A97TBNS9.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/9/UI/R0GL2BRVRLDHX8IAVJE09S.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/A/07/DCO3LJLICDI0J6M6JUBZNT.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/A/R7/21N5B0KCA16FLLAKCNCWRS.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/A/XO/KCHEPOD7KSTMWOYCUVL80D.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/A/Y2/JOMHOJFFZ4AYL3SGCRQQ4I.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/B/8S/8Z0JVOGBH5N0SHADXBSHYD.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/B/B5/UDRDGP8FMDAKJH9XEK5J32.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/B/CV/EMRSEA9V2AMZ4QTJV8ZXS5.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/B/EA/NOYU3U7LH6JQKAFB2MOGAV.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/B/EO/EMWX2HGK6U37EP4N40X7MN.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/B/JW/H5KYH0TR796PYQB2R58HQU.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/B/KK/E4G69FEBKIEJSFZNZWXT2M.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/B/U0/S8YLRK482DKN6LDPE5D34Z.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/0J/PKJX8CRJSUG3VO78YZB33Z.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/0O/Y4EFXZU31XXP74B0XOEBSD.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/1R/2MBNN2YL8T6F5MX4CZ3WXU.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/36/8THEGWEFDWR45SO7TY73JA.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/7X/SFT13K73RKN2LYEPJLI768.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/84/S1XVYHB2IATNSATED948E4.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/A7/13A5W6DL85LUTS70TM3NV3.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/FA/THIL6OWZUU3BJCYMKF5BNW.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/OP/3ZK6G82HWOH9QWXOTNDNJ2.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/C/X7/NMDCA2E2LG49TM7Q2VVZ6M.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/D/29/Y51JATUN9B9IM4MY2375QX.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/D/3M/ZJVBERW28T8M8U8D01RGO2.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/D/7R/A8NDU2I5T20CPVB4784W3E.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/D/8S/DZRCH7B8V4I9LA4TXZIIA7.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/D/JL/CPZC9EOS1FQTWKZY8ZJL3J.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/D/KE/NMZHTVOU58UMOED9QTCJ0O.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/D/PN/Q9ICZMAILP63KHQ27K6N5H.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/E/28/NPH3CPP0ZL0PGP0178SZ8O.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/E/D0/7J7KBII5HBKWHLJSZXITQS.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/E/EJ/IM3AZMUKQRB5S7G4T4L556.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/E/MS/NOQZSU27V5CVN4380HCQZE.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/E/ZR/JW4OFGAZLNSH5URGPCKL05.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/F/0T/KEGJKVZ214BH32QGBI873X.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/F/25/437GFIR6BKYTKHJESNUOVI.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/F/4U/OF0KJ35L96TD1NTJWVPXZF.uasset
Content/__ExternalActors__/Tests/Overload/Lvl_OverloadTest/F/4X/ASJKPD4ORAC9RSS1JZNVGB.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/4R/USWRROF0YGZZGWQ2CHNHKA.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/ED/GP2N37NOM204TM8HSIOHDA.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/H7/HXHBY8XB24FN2PZ9N5HJNL.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/KK/FN2AKQMCUI8N4G72TP2P2F.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/RQ/JQBEDFO07FYQTBAZRBESFQ.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/RT/UXR7VJZGTACYQ5KESB48LQ.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/SW/XDDW2UFRQWQEHQ45U62WCG.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/VX/WL3LKELHSK570W7KZPMJLL.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/WI/54CLCT4Y9TRDQT3H5CBBO2.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/1/Y4/CP52WV1UMP464PD8ICZOTA.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/2/0B/PMMHST2XKRV46LOJVX1P5K.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/2/1W/YHRY4WLD8BTNQ6QGV0P0HD.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/2/33/SKUR0WWL7JT7C3OXZMWAQS.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/2/EU/C5FXL3H56VG2BGRBVI945A.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/3/3W/KVKK3JZ9YOQDKZN4SA819U.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/3/DB/3WAT7AKB2FD7W2F2FBB3RL.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/4/3R/FBA0Z00NHOZRLY2VS6QWS5.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/4/AH/Q0LWU54DITS9SGJR66EGIS.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/4/EF/HK90174B848TUVF24GPVDP.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/4/LA/BYTOYHREOMLDWMZHV025W2.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/5/FO/ECPD0L2VGZ9HAW86J7D07O.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/5/MU/RF58KVN4H83EF3SOHF978Y.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/5/TK/BEGV01JEMC3BQKMHDEA7SB.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/38/VQ8HM2V1RTOU5IAUWY4WGW.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/AP/ZPKUEMBIIM2J5R0ENN58X3.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/C0/F9RZ6HOG7VEVHTE13KVK91.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/CE/DT9U6EGO7HSBDBDH26PHEU.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/MF/489IW7U7P9MIAKUIULAEPS.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/SX/UJD585HI4TXMW9YG4YAQO1.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/T0/BUK2FWFXTAPKHE13W1ZVWT.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/TH/Z8NDYBYXW69596N30WYMU7.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/6/VW/BJ8OS0VW6H2H0GK1FQS2F2.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/7/4B/T1PIVNI01BWDPTT6KC0IOK.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/7/6M/QTS3CWKKBCQU2OQX281KWZ.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/7/NA/4DMAZX2N566NQIXEI3N7ZM.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/7/PF/I0DNH0U6BTFFYSKQYHTIC2.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/8/PY/QMLKP6A3WDUNKON0OGPXW6.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/8/Y1/3VWCZVZL4ZW54PZAZFVD6U.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/9/OW/75DR5F3ICVIHE723FRDK10.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/9/V9/VCZ6Z3BRAFZ7RBG1Y9IVQD.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/9/XA/AKS4QGDKLNT2ECMCM0OSYJ.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/A/11/XDMMS42FJJ729ISOP9VU34.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/A/3I/L42FC3SQPNLGCNAE52AMJA.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/A/FM/E2Z2GWJUV6DYF4T64DU12Y.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/A/UI/FYPDNBRF80ZC1CFUDAJ8O0.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/B/HK/5C7FMPWMFNNW4N5A6KM7CJ.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/C/46/WDAXNYJP9CJF9H8ENZMQZ0.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/C/BP/BUPDHPEMHC4VKWBMQ8RZHT.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/C/CF/AT4JBJMPCJ00E8TPH8BEXB.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/C/QY/NWXGPXGH1INB3E31CV33D7.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/D/7X/F3A4HCV21GTJXPK2SLR5GG.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/D/SH/PW0AQVQUKM8YU4NRQMBZNU.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/D/Y3/PL6BFHEQZX9767CCT3NKJG.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/E/05/CUR46BAKJ3XVWLZ272OSZL.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/E/1J/HRXBHJMHKGUJ9O51YEGVI7.uasset
Content/__ExternalActors__/TopDown/Lvl_TopDown/E/P0/6BJ49D2QVGYTGKTJ12J1UB.uasset
Content/__ExternalObjects__/Tests/Overload/Lvl_OverloadTest/3/LE/VQU1IPC97UBIPCNI3C7YOY.uasset
Content/__ExternalObjects__/Tests/Overload/Lvl_OverloadTest/4/SE/UV9WMKZC3Z40VOEQAWSG1P.uasset
Content/__ExternalObjects__/TopDown/Lvl_TopDown/1/5F/ACU59NVHABC7KE5P5H7QYG.uasset
Content/__ExternalObjects__/TopDown/Lvl_TopDown/7/XH/ILKF82DAIKT5TPOED6FWR3.uasset
```

## 5. 待人工确认文件

这部分不会自动给出“可直接 Move”的结论：

```text
Content/3 -> [REVIEW]/Content/3
```

其中重点：

- `Content/3`：仓库中是约 100 MB 的无扩展名大文件。迁移前先确认用途；如果不是 Unreal 资产，应移出 `Content` 或删除。
- 如果后续扫描出现未匹配的新根目录，也先进入 Review，不做猜测式迁移。

## 6. 关键分类规则摘要

```text
GameMode                         -> ProjectArcaneArena/Core/GameMode
Core/BP_ArenaPlayerController   -> ProjectArcaneArena/Core/Controllers

Characters/ArenaPlayer          -> ProjectArcaneArena/Characters/Player
Characters/ArenaEnemy           -> ProjectArcaneArena/Characters/Enemies
Boss                             -> ProjectArcaneArena/Characters/Boss

GAS/GameplayAbility             -> ProjectArcaneArena/Combat/GAS/Abilities/{Player|Enemy|Triggers}
GAS/GameplayEffect              -> ProjectArcaneArena/Combat/GAS/Effects/{Core|Init|Cooldowns|Costs|Status|Triggers|Upgrades|Enemy}
GAS/GameplayCues                -> ProjectArcaneArena/Combat/GAS/Cues/{Instant|Looping|Elite}
GAS/Area                        -> ProjectArcaneArena/Combat/Areas/Dash
GAS/DamageFeedback              -> ProjectArcaneArena/Combat/Feedback/{Camera|Materials|Audio}
GAS/Projectile                  -> ProjectArcaneArena/Combat/Projectiles/Actors

Data/Weapon                     -> ProjectArcaneArena/Combat/Weapons/Data
Data/Upgrade                    -> ProjectArcaneArena/Systems/Upgrades/Data
Data/EnemyAffix                 -> ProjectArcaneArena/Characters/Enemies/Data/Affixes
Data/Pickup                     -> ProjectArcaneArena/Systems/Pickups/Data

Items/Inventory                 -> ProjectArcaneArena/Systems/Inventory
Items/Pickups                   -> ProjectArcaneArena/Systems/Pickups/Blueprints
Assets/Pickups                  -> ProjectArcaneArena/Systems/Pickups/Art

UI root HUD                     -> ProjectArcaneArena/UI/HUD
UI DamageNumber                 -> ProjectArcaneArena/UI/DamageNumbers
UI/MainMenu                     -> ProjectArcaneArena/UI/MainMenu
UI/Inventory                    -> ProjectArcaneArena/UI/Inventory
UI/UpgradeIcons (.uasset)      -> ProjectArcaneArena/UI/Upgrade/Icons\nUI/UpgradeIcons (raw PNG)       -> SourceArt/UI/UpgradeIcons
Cursor                          -> ProjectArcaneArena/UI/Cursor/Current

TopDown/Input                   -> ProjectArcaneArena/Input
TopDown/Lvl_TopDown             -> ProjectArcaneArena/World/Maps/Lvl_Arena
TopDown/MI_Colorway             -> ProjectArcaneArena/World/Materials
TopDown/Blueprints + Cursor     -> ProjectArcaneArena/Dev/LegacyTemplate/TopDown

Tests                           -> ProjectArcaneArena/Dev/Tests
Mass                            -> ProjectArcaneArena/Dev/Experiments/Mass
Niagara                         -> ProjectArcaneArena/VFX/Common

Python                          -> Scripts/Python
mcp-conversation-export.md      -> Docs/Archive
Stylized_Spruce_Forest          -> 保留原位
LevelPrototyping                -> 保留原位
__ExternalActors__/Objects      -> UE 自动管理
```

## 7. 完整性检查

本文档生成时满足：

```text
当前纳入 Content 文件数 = 748
目标手工目录文件数       = 585
UE 自动管理文件数         = 162
待人工确认文件数          = 1

585 + 162 + 1 = 748
```

应始终满足最后一行等于当前纳入文件数 748。后续新增/删除资产后应重新生成本清单，而不是手工假设它仍然完整。
