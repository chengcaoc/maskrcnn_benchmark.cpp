#include "box_coder.h"


namespace rcnn{
namespace modeling{
  
  BoxCoder::BoxCoder(std::vector<float> weights, double bbox_xform_clip)
                    :weights_(weights),
                     bbox_xform_clip_(bbox_xform_clip){}
  
  torch::Tensor BoxCoder::encode(torch::Tensor reference_boxes, torch::Tensor proposals){
    int TO_REMOVE = 1;
    torch::Tensor ex_widths = proposals.slice(1, 2, 3) - proposals.slice(1, 0, 1) + TO_REMOVE;
    torch::Tensor ex_heights = proposals.slice(1, 3, 4) - proposals.slice(1, 1, 2) + TO_REMOVE;
    torch::Tensor ex_ctr_x = proposals.slice(1, 0, 1) + 0.5 * ex_widths;
    torch::Tensor ex_ctr_y = proposals.slice(1, 1, 2) + 0.5 * ex_heights;

    torch::Tensor gt_widths = reference_boxes.slice(1, 2, 3) - reference_boxes.slice(1, 0, 1) + TO_REMOVE;
    torch::Tensor gt_heights = reference_boxes.slice(1, 3, 4) - reference_boxes.slice(1, 1, 2) + TO_REMOVE;
    torch::Tensor gt_ctr_x = reference_boxes.slice(1, 0, 1) + 0.5 * gt_widths;
    torch::Tensor gt_ctr_y = reference_boxes.slice(1, 1, 2) + 0.5 * gt_heights;

    return torch::stack({
      /*targets_dx*/ weights_[0] * (gt_ctr_x - ex_ctr_x) / ex_widths,
      /*targets_dy*/ weights_[1] * (gt_ctr_y - ex_ctr_y) / ex_heights,
      /*targets_dw*/ weights_[2] * torch::log(gt_widths / ex_widths),
      /*targets_dh*/ weights_[3] * torch::log(gt_heights / ex_heights)
    }, /*dim=*/1);
  }

  torch::Tensor BoxCoder::decode(torch::Tensor rel_codes, torch::Tensor boxes){
    int TO_REMOVE = 1;
    torch::Tensor widths = boxes.slice(1, 2, 3) - boxes.slice(1, 0, 1) + TO_REMOVE;
    torch::Tensor heights = boxes.slice(1, 3, 4) - boxes.slice(1, 1, 2) + TO_REMOVE;
    torch::Tensor ctr_x = boxes.slice(1, 0, 1) + 0.5 * widths;
    torch::Tensor ctr_y = boxes.slice(1, 1, 2) + 0.5 * heights;
    
    auto length = rel_codes.size(1);
    torch::Tensor dx = rel_codes.slice(1, 0, length, /*step=*/4) / weights_[0];
    torch::Tensor dy = rel_codes.slice(1, 1, length, /*step=*/4) / weights_[1];
    torch::Tensor dw = rel_codes.slice(1, 2, length, /*step=*/4) / weights_[2];
    torch::Tensor dh = rel_codes.slice(1, 3, length, /*step=*/4) / weights_[3];

    dw = torch::clamp(dw, /*max=*/bbox_xform_clip_);
    dh = torch::clamp(dh, /*max=*/bbox_xform_clip_);

    torch::Tensor pred_ctr_x = dx * widths.unsqueeze(1) + ctr_x.unsqueeze(1);
    torch::Tensor pred_ctr_y = dy * heights.unsqueeze(1) + ctr_y.unsqueeze(1);
    torch::Tensor pred_w = torch::exp(dw) * widths.unsqueeze(1);
    torch::Tensor pred_h = torch::exp(dh) * heights.unsqueeze(1);

    torch::Tensor pred_boxes = torch::zeros_like(rel_codes);
    pred_boxes.slice(1, 0, length, 4).copy_(pred_ctr_x - 0.5 * pred_w);
    pred_boxes.slice(1, 1, length, 4).copy_(pred_ctr_y - 0.5 * pred_h);
    pred_boxes.slice(1, 2, length, 4).copy_(pred_ctr_x + 0.5 * pred_w - 1);
    pred_boxes.slice(1, 3, length, 4).copy_(pred_ctr_y + 0.5 * pred_h - 1);

    return pred_boxes;
  }
  
}
}