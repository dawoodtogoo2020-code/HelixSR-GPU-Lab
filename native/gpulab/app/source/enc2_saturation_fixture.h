/* Generated diagnostic only. CPU expected files are never uploads. */
#ifndef ENC2_SATURATION_FIXTURE_H
#define ENC2_SATURATION_FIXTURE_H
#include "model_sequence.h"
#define SAT_WARMUP_FRAMES 1u
#define SAT_MEASURED_FRAMES 3u
static const struct ms_resource sat_baseline_resources[]={
{"input",{"common/input-b5.bin","d061a4ab2dc67147b0a1e75f68a812f03e8511958573250796b987525f1c30f0",4096},MS_BUFFER,MS_IMMUTABLE,0,0,0},
{"weights",{"common/weights.bin","f2f09558a2fbfa1dd1c20a96ec1620c68b37b0958512fa16ad32a9e2da94a7cd",1903872},MS_BUFFER,MS_IMMUTABLE,0,0,0},
{"b7",{"baseline/initial-b7.bin","ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7",4096},MS_BUFFER,0,0,0,0},
{"b4",{"baseline/initial-b4.bin","39c87779d3c21f3b187e071dd9520a9f7df496c2b8e7af5a251ff3d3e8ec1f6a",81920},MS_BUFFER,0,0,0,0}};
static const struct ms_role sat_baseline_roles[]={{"input",0,0},{"weights",1,1},{"b7",2,2},{"b4",3,3}};
static const struct ms_shader sat_baseline_shaders[]={{"enc2",{"common/baseline-code.bin","b76f530262b803ad2304bcc28fa8838d7f88307b9fede1aacfbb061f1c47f5b7",439172},1076625435u,1573254u,{128,1,1},2,0}};
static const struct ms_binding sat_baseline_bindings[]={
{MS_DESC_BUFFER,0,MS_ARGS_ROLE,0,416,{0}},
{MS_DESC_BUFFER,16,0,0,4096,{0}},
{MS_DESC_BUFFER,32,1,161536,1742336,{0}},
{MS_DESC_BUFFER,48,1,456448,1447424,{0}},
{MS_DESC_BUFFER,64,2,0,4096,{0}},
{MS_DESC_BUFFER,80,3,16384,65536,{0}}};
static const struct ms_checkpoint sat_baseline_checks[]={{"B7",2,0,{"baseline/expected-b7.bin","a7755302622fc3e39aa93d2de8cf288dc7308c4dfe8b396bfc32c0bee5ec7a2b",4096},MS_EXACT,0},{"B4",3,0,{"baseline/expected-b4.bin","0d515124b865ffac51d2b8b2b051779caf491ec6e025d2900cb595f2c01b8d77",81920},MS_EXACT,0}};
static const struct ms_upload sat_baseline_uploads[]={{2,0,{"baseline/initial-b7.bin","ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7",4096}},{3,0,{"baseline/initial-b4.bin","39c87779d3c21f3b187e071dd9520a9f7df496c2b8e7af5a251ff3d3e8ec1f6a",81920}}};
static const struct ms_launch sat_baseline_launches[]={
{"baseline-warmup",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,1},96,sat_baseline_bindings,6,sat_baseline_checks,2},
{"baseline-sample1",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,1},96,sat_baseline_bindings,6,sat_baseline_checks,2},
{"baseline-sample2",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,1},96,sat_baseline_bindings,6,sat_baseline_checks,2},
{"baseline-sample3",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,1},96,sat_baseline_bindings,6,sat_baseline_checks,2},
};
static const struct ms_frame sat_baseline_frames[]={
{"baseline-warmup",0,1,sat_baseline_uploads,2,NULL,0},
{"baseline-sample1",1,1,sat_baseline_uploads,2,NULL,0},
{"baseline-sample2",2,1,sat_baseline_uploads,2,NULL,0},
{"baseline-sample3",3,1,sat_baseline_uploads,2,NULL,0},
};
static const struct ms_fixture sat_baseline_fixture={MS_FIXTURE_ABI,"enc2-saturation-baseline",sat_baseline_resources,4,sat_baseline_roles,4,sat_baseline_shaders,1,sat_baseline_launches,4,sat_baseline_frames,4,1000000000ull};
static const struct ms_resource sat_n1_resources[]={
{"input",{"common/input-b5.bin","d061a4ab2dc67147b0a1e75f68a812f03e8511958573250796b987525f1c30f0",4096},MS_BUFFER,MS_IMMUTABLE,0,0,0},
{"weights",{"common/weights.bin","f2f09558a2fbfa1dd1c20a96ec1620c68b37b0958512fa16ad32a9e2da94a7cd",1903872},MS_BUFFER,MS_IMMUTABLE,0,0,0},
{"b7",{"n1/initial-b7.bin","e7baf3fffd9fe3e484fb73a8abe847283775699e2f9bc53e71990fd0912e84c6",4352},MS_BUFFER,0,0,0,0},
{"b4",{"n1/initial-b4.bin","4c5f132d768083261c1b6fbf654f38f5f5fb5df7780add35bf8ba2e51a43483d",82176},MS_BUFFER,0,0,0,0}};
static const struct ms_role sat_n1_roles[]={{"input",0,0},{"weights",1,1},{"b7",2,2},{"b4",3,3}};
static const struct ms_shader sat_n1_shaders[]={{"enc2",{"common/batched-code.bin","3c638f178aaa31e20638420fd5c097af6bad2d326a7351a577e14c13f02d9cb0",439460},1076625435u,1573766u,{128,1,1},2,0}};
static const struct ms_binding sat_n1_bindings[]={
{MS_DESC_BUFFER,0,MS_ARGS_ROLE,0,416,{0}},
{MS_DESC_BUFFER,16,0,0,4096,{0}},
{MS_DESC_BUFFER,32,1,161536,1742336,{0}},
{MS_DESC_BUFFER,48,1,456448,1447424,{0}},
{MS_DESC_BUFFER,64,2,0,4352,{0}},
{MS_DESC_BUFFER,80,3,16384,65792,{0}}};
static const struct ms_checkpoint sat_n1_checks[]={{"B7",2,0,{"n1/expected-b7.bin","91863f4574a046d5d2dc5c8a8d651bfd57fe385dbd331524cb02d4c27f841cc2",4352},MS_EXACT,0},{"B4",3,0,{"n1/expected-b4.bin","faa26248424f6d0677ddff31b611c9f40157eaea46608485aca8c07105d22b6c",82176},MS_EXACT,0}};
static const struct ms_upload sat_n1_uploads[]={{2,0,{"n1/initial-b7.bin","e7baf3fffd9fe3e484fb73a8abe847283775699e2f9bc53e71990fd0912e84c6",4352}},{3,0,{"n1/initial-b4.bin","4c5f132d768083261c1b6fbf654f38f5f5fb5df7780add35bf8ba2e51a43483d",82176}}};
static const struct ms_launch sat_n1_launches[]={
{"n1-warmup",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,1},96,sat_n1_bindings,6,sat_n1_checks,2},
{"n1-sample1",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,1},96,sat_n1_bindings,6,sat_n1_checks,2},
{"n1-sample2",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,1},96,sat_n1_bindings,6,sat_n1_checks,2},
{"n1-sample3",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,1},96,sat_n1_bindings,6,sat_n1_checks,2},
};
static const struct ms_frame sat_n1_frames[]={
{"n1-warmup",0,1,sat_n1_uploads,2,NULL,0},
{"n1-sample1",1,1,sat_n1_uploads,2,NULL,0},
{"n1-sample2",2,1,sat_n1_uploads,2,NULL,0},
{"n1-sample3",3,1,sat_n1_uploads,2,NULL,0},
};
static const struct ms_fixture sat_n1_fixture={MS_FIXTURE_ABI,"enc2-saturation-n1",sat_n1_resources,4,sat_n1_roles,4,sat_n1_shaders,1,sat_n1_launches,4,sat_n1_frames,4,1000000000ull};
static const struct ms_resource sat_n8_resources[]={
{"input",{"common/input-b5.bin","d061a4ab2dc67147b0a1e75f68a812f03e8511958573250796b987525f1c30f0",4096},MS_BUFFER,MS_IMMUTABLE,0,0,0},
{"weights",{"common/weights.bin","f2f09558a2fbfa1dd1c20a96ec1620c68b37b0958512fa16ad32a9e2da94a7cd",1903872},MS_BUFFER,MS_IMMUTABLE,0,0,0},
{"b7",{"n8/initial-b7.bin","ac286e1e9d9da0cd15f74ae5b1afeecd2f1d2dbf84107d26ba91200994e9dfbd",34816},MS_BUFFER,0,0,0,0},
{"b4",{"n8/initial-b4.bin","00e5e81f63d8fed68ce131470e5397a66ba4bd64aff3b748d010f41472f4ad52",657408},MS_BUFFER,0,0,0,0}};
static const struct ms_role sat_n8_roles[]={{"input",0,0},{"weights",1,1},{"b7",2,2},{"b4",3,3}};
static const struct ms_shader sat_n8_shaders[]={{"enc2",{"common/batched-code.bin","3c638f178aaa31e20638420fd5c097af6bad2d326a7351a577e14c13f02d9cb0",439460},1076625435u,1573766u,{128,1,1},2,0}};
static const struct ms_binding sat_n8_bindings[]={
{MS_DESC_BUFFER,0,MS_ARGS_ROLE,0,416,{0}},
{MS_DESC_BUFFER,16,0,0,4096,{0}},
{MS_DESC_BUFFER,32,1,161536,1742336,{0}},
{MS_DESC_BUFFER,48,1,456448,1447424,{0}},
{MS_DESC_BUFFER,64,2,0,34816,{0}},
{MS_DESC_BUFFER,80,3,16384,641024,{0}}};
static const struct ms_checkpoint sat_n8_checks[]={{"B7",2,0,{"n8/expected-b7.bin","39b5c505879ba1f59d4f9dcde36bb7a050614e753de4efa26ed4cee5923a2cfe",34816},MS_EXACT,0},{"B4",3,0,{"n8/expected-b4.bin","fa9a39f35c9d7a818f98e787201388706a2681d793696ef737c8491010ae3e95",657408},MS_EXACT,0}};
static const struct ms_upload sat_n8_uploads[]={{2,0,{"n8/initial-b7.bin","ac286e1e9d9da0cd15f74ae5b1afeecd2f1d2dbf84107d26ba91200994e9dfbd",34816}},{3,0,{"n8/initial-b4.bin","00e5e81f63d8fed68ce131470e5397a66ba4bd64aff3b748d010f41472f4ad52",657408}}};
static const struct ms_launch sat_n8_launches[]={
{"n8-warmup",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,8},96,sat_n8_bindings,6,sat_n8_checks,2},
{"n8-sample1",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,8},96,sat_n8_bindings,6,sat_n8_checks,2},
{"n8-sample2",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,8},96,sat_n8_bindings,6,sat_n8_checks,2},
{"n8-sample3",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,8},96,sat_n8_bindings,6,sat_n8_checks,2},
};
static const struct ms_frame sat_n8_frames[]={
{"n8-warmup",0,1,sat_n8_uploads,2,NULL,0},
{"n8-sample1",1,1,sat_n8_uploads,2,NULL,0},
{"n8-sample2",2,1,sat_n8_uploads,2,NULL,0},
{"n8-sample3",3,1,sat_n8_uploads,2,NULL,0},
};
static const struct ms_fixture sat_n8_fixture={MS_FIXTURE_ABI,"enc2-saturation-n8",sat_n8_resources,4,sat_n8_roles,4,sat_n8_shaders,1,sat_n8_launches,4,sat_n8_frames,4,1000000000ull};
static const struct ms_resource sat_n32_resources[]={
{"input",{"common/input-b5.bin","d061a4ab2dc67147b0a1e75f68a812f03e8511958573250796b987525f1c30f0",4096},MS_BUFFER,MS_IMMUTABLE,0,0,0},
{"weights",{"common/weights.bin","f2f09558a2fbfa1dd1c20a96ec1620c68b37b0958512fa16ad32a9e2da94a7cd",1903872},MS_BUFFER,MS_IMMUTABLE,0,0,0},
{"b7",{"n32/initial-b7.bin","05e9b0666098984a60799667d1c73a2a3e2a195ec15797f9c962a2fcc4f202fb",139264},MS_BUFFER,0,0,0,0},
{"b4",{"n32/initial-b4.bin","c0c5754792ae80ca0a62aa08d7ad8667f7376f05a656083db5c86948a34d82fd",2629632},MS_BUFFER,0,0,0,0}};
static const struct ms_role sat_n32_roles[]={{"input",0,0},{"weights",1,1},{"b7",2,2},{"b4",3,3}};
static const struct ms_shader sat_n32_shaders[]={{"enc2",{"common/batched-code.bin","3c638f178aaa31e20638420fd5c097af6bad2d326a7351a577e14c13f02d9cb0",439460},1076625435u,1573766u,{128,1,1},2,0}};
static const struct ms_binding sat_n32_bindings[]={
{MS_DESC_BUFFER,0,MS_ARGS_ROLE,0,416,{0}},
{MS_DESC_BUFFER,16,0,0,4096,{0}},
{MS_DESC_BUFFER,32,1,161536,1742336,{0}},
{MS_DESC_BUFFER,48,1,456448,1447424,{0}},
{MS_DESC_BUFFER,64,2,0,139264,{0}},
{MS_DESC_BUFFER,80,3,16384,2613248,{0}}};
static const struct ms_checkpoint sat_n32_checks[]={{"B7",2,0,{"n32/expected-b7.bin","f33e8f6728028a2d586cab49e4c001a8a5563df075258d5fe05a6842767e70d3",139264},MS_EXACT,0},{"B4",3,0,{"n32/expected-b4.bin","0701099dac4b7a38409cafc79a19666e54d5f56fc5c1469f741078f702b572d4",2629632},MS_EXACT,0}};
static const struct ms_upload sat_n32_uploads[]={{2,0,{"n32/initial-b7.bin","05e9b0666098984a60799667d1c73a2a3e2a195ec15797f9c962a2fcc4f202fb",139264}},{3,0,{"n32/initial-b4.bin","c0c5754792ae80ca0a62aa08d7ad8667f7376f05a656083db5c86948a34d82fd",2629632}}};
static const struct ms_launch sat_n32_launches[]={
{"n32-warmup",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,32},96,sat_n32_bindings,6,sat_n32_checks,2},
{"n32-sample1",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,32},96,sat_n32_bindings,6,sat_n32_checks,2},
{"n32-sample2",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,32},96,sat_n32_bindings,6,sat_n32_checks,2},
{"n32-sample3",0,{"common/args.bin","47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad",416},{2,1,32},96,sat_n32_bindings,6,sat_n32_checks,2},
};
static const struct ms_frame sat_n32_frames[]={
{"n32-warmup",0,1,sat_n32_uploads,2,NULL,0},
{"n32-sample1",1,1,sat_n32_uploads,2,NULL,0},
{"n32-sample2",2,1,sat_n32_uploads,2,NULL,0},
{"n32-sample3",3,1,sat_n32_uploads,2,NULL,0},
};
static const struct ms_fixture sat_n32_fixture={MS_FIXTURE_ABI,"enc2-saturation-n32",sat_n32_resources,4,sat_n32_roles,4,sat_n32_shaders,1,sat_n32_launches,4,sat_n32_frames,4,1000000000ull};
#endif
